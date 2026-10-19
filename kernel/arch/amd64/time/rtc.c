#include <arch/amd64.pmio.h>
#include <arch/hart.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <vex/calendar.h>
#include <vex/errno.h>
#include <vex/rtc.h>
#include <vex/spinlock.h>
#include <vex/string.h>

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71

#define RTC_SECONDS  0x00
#define RTC_MINUTES  0x02
#define RTC_HOURS    0x04
#define RTC_DAY      0x07
#define RTC_MONTH    0x08
#define RTC_YEAR     0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

#define RTC_A_UIP    0x80 // Update in progress, the time registers are changing
#define RTC_B_SET    0x80 // Updates stopped while the time is written
#define RTC_B_24HOUR 0x02
#define RTC_B_BINARY 0x04
#define RTC_HOUR_PM  0x80 // In the 12-hour format only

#define RTC_READ_TRIES 10
#define RTC_UIP_SPINS  100000 // An update lasts about 2 ms, far less than this many port reads

#define RTC_TWO_DIGIT_PIVOT 70 // Without a century register, years below this are 20xx

struct cmos_fields {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;
};

static spinlock_type cmos_lock = SPINLOCK_INIT;

static uint8_t cmos_read(uint8_t reg)
{
    pmwrite8(CMOS_INDEX, reg);

    return pmread8(CMOS_DATA);
}

static void cmos_write(uint8_t reg, uint8_t value)
{
    pmwrite8(CMOS_INDEX, reg);
    pmwrite8(CMOS_DATA, value);
}

static unsigned from_bcd(uint8_t value)
{
    return (value >> 4) * 10 + (value & 0x0F);
}

static uint8_t to_bcd(unsigned value)
{
    return (uint8_t)((value / 10) << 4 | value % 10);
}

// The FADT names the century register, if there is one, and says
// when a PC has no CMOS clock at all; without ACPI it is a plain PC
static int probe_cmos(uint8_t* restrict century_reg)
{
    struct acpi_fadt* fadt;
    *century_reg = 0;

    if(uacpi_unlikely_error(uacpi_table_fadt(&fadt))) {
        return 0;
    }

    if(fadt->iapc_boot_arch & ACPI_IA_PC_NO_CMOS_RTC) {
        return -ENODEV;
    }

    *century_reg = fadt->century;

    return 0;
}

static void read_fields_locked(uint8_t century_reg, struct cmos_fields* restrict out)
{
    for(unsigned i = 0; i < RTC_UIP_SPINS && (RTC_A_UIP & cmos_read(RTC_STATUS_A)); ++i) {
        hart_relax();
    }

    out->second = cmos_read(RTC_SECONDS);
    out->minute = cmos_read(RTC_MINUTES);
    out->hour = cmos_read(RTC_HOURS);
    out->day = cmos_read(RTC_DAY);
    out->month = cmos_read(RTC_MONTH);
    out->year = cmos_read(RTC_YEAR);
    out->century = 0;

    if(century_reg) {
        out->century = cmos_read(century_reg);
    }
}

static unsigned decode(uint8_t value, bool binary)
{
    if(binary) {
        return value;
    }

    return from_bcd(value);
}

static uint8_t encode(unsigned value, bool binary)
{
    if(binary) {
        return (uint8_t)(value);
    }

    return to_bcd(value);
}

int arch_rtc_read(time_t* restrict out)
{
    uint8_t century_reg;
    int retval = probe_cmos(&century_reg);

    if(retval < 0) {
        return retval;
    }

    struct cmos_fields fields;
    struct cmos_fields again;

    bool intr_state = spinlock_acquire_intr(&cmos_lock);

    // Two equal reads in a row can't have
    // straddled an update of the registers
    read_fields_locked(century_reg, &fields);

    for(unsigned i = 0; i < RTC_READ_TRIES; ++i) {
        read_fields_locked(century_reg, &again);

        if(0 == memcmp(&fields, &again, sizeof(fields))) {
            break;
        }

        fields = again;
    }

    uint8_t status_b = cmos_read(RTC_STATUS_B);

    spinlock_release_intr(&cmos_lock, intr_state);

    bool binary = status_b & RTC_B_BINARY;
    bool pm = 0 == (status_b & RTC_B_24HOUR) && (fields.hour & RTC_HOUR_PM);

    struct calendar_time calendar = {
        .second = decode(fields.second, binary),
        .minute = decode(fields.minute, binary),
        .hour = decode(fields.hour & ~RTC_HOUR_PM, binary),
        .day = decode(fields.day, binary),
        .month = decode(fields.month, binary),
        .year = decode(fields.year, binary),
    };

    if(0 == (status_b & RTC_B_24HOUR)) {
        calendar.hour %= 12;

        if(pm) {
            calendar.hour += 12;
        }
    }

    if(century_reg) {
        calendar.year += decode(fields.century, binary) * 100;
    }
    else if(calendar.year < RTC_TWO_DIGIT_PIVOT) {
        calendar.year += 2000;
    }
    else {
        calendar.year += 1900;
    }

    if(calendar.month < 1 || calendar.month > 12 || calendar.day < 1 || calendar.day > 31) {
        return -EIO;
    }

    if(calendar.hour > 23 || calendar.minute > 59 || calendar.second > 59) {
        return -EIO;
    }

    *out = calendar_to_time(&calendar);

    return 0;
}

int arch_rtc_write(time_t time)
{
    uint8_t century_reg;
    int retval = probe_cmos(&century_reg);

    if(retval < 0) {
        return retval;
    }

    struct calendar_time calendar;
    calendar_from_time(time, &calendar);

    int64_t first_year = 1900 + RTC_TWO_DIGIT_PIVOT;
    int64_t last_year = first_year + 99;

    if(century_reg) {
        first_year = 0;
        last_year = 9999;
    }

    if(calendar.year < first_year || calendar.year > last_year) {
        return -EINVAL;
    }

    bool intr_state = spinlock_acquire_intr(&cmos_lock);

    uint8_t status_b = cmos_read(RTC_STATUS_B);
    bool binary = status_b & RTC_B_BINARY;
    uint8_t hour = encode(calendar.hour, binary);

    if(0 == (status_b & RTC_B_24HOUR)) {
        unsigned hour_12 = calendar.hour % 12;

        if(hour_12 == 0) {
            hour_12 = 12;
        }

        hour = encode(hour_12, binary);

        if(calendar.hour >= 12) {
            hour |= RTC_HOUR_PM;
        }
    }

    cmos_write(RTC_STATUS_B, status_b | RTC_B_SET);

    cmos_write(RTC_SECONDS, encode(calendar.second, binary));
    cmos_write(RTC_MINUTES, encode(calendar.minute, binary));
    cmos_write(RTC_HOURS, hour);
    cmos_write(RTC_DAY, encode(calendar.day, binary));
    cmos_write(RTC_MONTH, encode(calendar.month, binary));
    cmos_write(RTC_YEAR, encode((unsigned)(calendar.year % 100), binary));

    if(century_reg) {
        cmos_write(century_reg, encode((unsigned)(calendar.year / 100), binary));
    }

    cmos_write(RTC_STATUS_B, status_b & ~RTC_B_SET);

    spinlock_release_intr(&cmos_lock, intr_state);

    return 0;
}
