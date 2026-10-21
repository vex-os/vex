#include <vex/ctype.h>
#include <vex/errno.h>
#include <vex/limits.h>
#include <vex/pslab.h>
#include <vex/signal.h>
#include <vex/string.h>
#include <vex/termios.h>
#include <vex/tty_discipline.h>

#define TAB_WIDTH 8
#define DELETE    0x7F
#define BELL      0x07
#define MARK      0xFF // PARMRK reads a byte with an error as \377 \0 and the byte, and a \377 received as \377 \377

#define CONTROL_FLIP 0x40 // Turns a control byte into the letter echoed after ^, and back

// IXOFF stops the sender when the input has less room than STOP_ROOM, a margin for
// what the sender and the flip ring hold and lets it go on at START_ROOM
#define STOP_ROOM  256U
#define START_ROOM (TTY_RING_SIZE / 2)

static bool is_utf8_continuation(uint8_t byte)
{
    return 0x80 == (byte & 0xC0);
}

static bool is_special(const termios_t* restrict termios, uint8_t byte, unsigned index)
{
    uint8_t special = termios->c_cc[index];
    return special != _POSIX_VDISABLE && special == byte;
}

// XCASE stands a backslash and one of these for
// a character an uppercase-only terminal lacks
static uint8_t xcase_escape(uint8_t byte)
{
    switch(byte) {
        case '{':
            return '(';
        case '}':
            return ')';
        case '|':
            return '!';
        case '~':
            return '^';
        case '`':
            return '\'';
        default:
            return 0;
    }
}

static uint8_t xcase_unescape(uint8_t byte)
{
    switch(byte) {
        case '(':
            return '{';
        case ')':
            return '}';
        case '!':
            return '|';
        case '^':
            return '~';
        case '\'':
            return '`';
        default:
            return 0;
    }
}

// Fill bytes stand in for the delays; at 110 baud
// each one takes a tenth of a second. A delay field
// over its lowest bit, such as CRDLY over CR1, is the delay's number
static unsigned delay_fills(uint32_t oflag, uint8_t byte)
{
    switch(byte) {
        case '\n':
            return (oflag & NLDLY) / NL1;
        case '\r':
            return (oflag & CRDLY) / CR1;
        case '\t':
            return (oflag & TABDLY) / TAB1;
        case '\b':
            return (oflag & BSDLY) / BS1;
        case '\v':
            return 2 * ((oflag & VTDLY) / VT1);
        case '\f':
            return 2 * ((oflag & FFDLY) / FF1);
        default:
            return 0;
    }
}

static uint32_t next_tab_stop(uint32_t column)
{
    return (column / TAB_WIDTH + 1) * TAB_WIDTH;
}

// Columns a byte of the line took when it was echoed
static unsigned echo_width(const termios_t* restrict termios, uint8_t byte)
{
    if(iscntrl(byte)) {
        if(termios->c_lflag & ECHOCTL) {
            return 2;
        }

        return 0;
    }

    if((termios->c_iflag & IUTF8) && is_utf8_continuation(byte)) {
        return 0;
    }

    return 1;
}

static size_t expand_printable(const termios_t* restrict termios, uint8_t byte, uint8_t* restrict out, uint32_t* restrict column)
{
    size_t length = 0;

    if((termios->c_lflag & (XCASE | ICANON)) == (XCASE | ICANON)) {
        uint8_t escape = xcase_escape(byte);

        if(isupper(byte)) {
            out[length] = '\\';
            length += 1;
        }
        else if(escape) {
            out[length] = '\\';
            length += 1;
            byte = escape;
        }
    }

    if(termios->c_oflag & OLCUC) {
        byte = toupper(byte);
    }

    out[length] = byte;
    length += 1;

    if(!iscntrl(byte) && !((termios->c_iflag & IUTF8) && is_utf8_continuation(byte))) {
        *column += length;
    }

    return length;
}

static size_t expand(const termios_t* restrict termios, uint8_t byte, uint8_t* restrict out, uint32_t* restrict column)
{
    uint32_t oflag = termios->c_oflag;

    if(0 == (oflag & OPOST)) {
        out[0] = byte;

        if(echo_width(termios, byte) == 1) {
            *column += 1;
        }

        return 1;
    }

    size_t length = 0;

    if(byte == '\n') {
        if(oflag & ONLCR) {
            out[length] = '\r';
            length += 1;
            *column = 0;
        }
        else if(oflag & ONLRET) {
            *column = 0;
        }

        out[length] = '\n';
        length += 1;
    }
    else if(byte == '\r') {
        if((oflag & ONOCR) && *column == 0) {
            return 0;
        }

        if(oflag & OCRNL) {
            byte = '\n';

            if(oflag & ONLRET) {
                *column = 0;
            }
        }
        else {
            *column = 0;
        }

        out[length] = byte;
        length += 1;
    }
    else if(byte == '\t') {
        uint32_t stop = next_tab_stop(*column);

        if((oflag & TABDLY) == TAB3) {
            while(*column < stop) {
                out[length] = ' ';
                length += 1;
                *column += 1;
            }

            return length;
        }

        *column = stop;
        out[length] = '\t';
        length += 1;
    }
    else if(byte == '\b') {
        if(*column > 0) {
            *column -= 1;
        }

        out[length] = '\b';
        length += 1;
    }
    else if(byte == '\v' || byte == '\f') {
        out[length] = byte;
        length += 1;
    }
    else {
        return expand_printable(termios, byte, out, column);
    }

    if(oflag & OFILL) {
        uint8_t fill = 0;

        if(oflag & OFDEL) {
            fill = DELETE;
        }

        for(unsigned i = delay_fills(oflag, byte); i > 0; --i) {
            out[length] = fill;
            length += 1;
        }
    }

    return length;
}

static void output_or_drop(tty_device_t* restrict device, uint8_t byte)
{
    // Echo only: the user sees less, but the input is kept
    (void)tty_discipline_output(device, byte);
}

static void echo(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;

    if((termios->c_lflag & ECHOCTL) && iscntrl(byte) && byte != '\t' && byte != '\n') {
        output_or_drop(device, '^');
        output_or_drop(device, byte ^ CONTROL_FLIP);
        return;
    }

    output_or_drop(device, byte);
}

static void echo_string(tty_device_t* restrict device, const char* restrict string)
{
    while(*string) {
        output_or_drop(device, *string);
        string += 1;
    }
}

static bool delimiter_at(const tty_discipline_state_t* restrict discipline, uint32_t position)
{
    uint32_t bit = position % TTY_RING_SIZE;
    return discipline->delimiters[bit / 8] & (1U << (bit % 8));
}

static void store(tty_device_t* restrict device, uint8_t byte, bool delimiter)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;
    tty_ring_t* input = &discipline->input;
    uint32_t bit = input->tail % TTY_RING_SIZE;

    if(delimiter) {
        discipline->delimiters[bit / 8] |= 1U << (bit % 8);
    }
    else {
        discipline->delimiters[bit / 8] &= ~(1U << (bit % 8));
    }

    tty_ring_put(input, byte);
}

static void close_hardcopy_erase(tty_device_t* restrict device)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;

    if(discipline->hardcopy_erasing) {
        discipline->hardcopy_erasing = false;
        echo(device, '/');
    }
}

static uint32_t line_width(const tty_device_t* restrict device, uint32_t end)
{
    const termios_t* termios = &device->priv.termios;
    const tty_discipline_state_t* discipline = &device->priv.discipline;
    uint32_t column = discipline->line_column;

    for(uint32_t position = discipline->line_end; position != end; position += 1) {
        uint8_t byte = tty_ring_at(&discipline->input, position);

        if(byte == '\t') {
            column = next_tab_stop(column);
        }
        else {
            column += echo_width(termios, byte);
        }
    }

    return column;
}

static void echo_erase(tty_device_t* restrict device, uint32_t erased_at, uint32_t erased_end)
{
    const termios_t* termios = &device->priv.termios;
    tty_discipline_state_t* discipline = &device->priv.discipline;

    if(termios->c_lflag & ECHOPRT) {
        if(!discipline->hardcopy_erasing) {
            discipline->hardcopy_erasing = true;
            echo(device, '\\');
        }

        for(uint32_t position = erased_at; position != erased_end; position += 1) {
            echo(device, tty_ring_at(&discipline->input, position));
        }

        return;
    }

    uint8_t lead = tty_ring_at(&discipline->input, erased_at);

    if(lead == '\t') {
        uint32_t target = line_width(device, erased_at);

        for(uint32_t i = discipline->column - target; i > 0 && discipline->column > target; --i) {
            output_or_drop(device, '\b');
        }

        return;
    }

    for(unsigned i = echo_width(termios, lead); i > 0; --i) {
        echo_string(device, "\b \b");
    }
}

// The bytes stay in the ring past the
// tail so the echo can still look at them
static bool erase_last(tty_device_t* restrict device)
{
    const termios_t* termios = &device->priv.termios;
    tty_discipline_state_t* discipline = &device->priv.discipline;
    tty_ring_t* input = &discipline->input;
    uint32_t end = input->tail;

    if(input->tail == discipline->line_end) {
        return false;
    }

    input->tail -= 1;

    while((termios->c_iflag & IUTF8) && input->tail != discipline->line_end && is_utf8_continuation(tty_ring_at(input, input->tail))) {
        input->tail -= 1;
    }

    if((termios->c_lflag & (ECHO | ECHOE)) == (ECHO | ECHOE) || (termios->c_lflag & (ECHO | ECHOPRT)) == (ECHO | ECHOPRT)) {
        echo_erase(device, input->tail, end);
    }

    return true;
}

static bool erases_visibly(const termios_t* restrict termios)
{
    return (bool)(termios->c_lflag & (ECHOE | ECHOPRT));
}

static void erase(tty_device_t* restrict device, uint8_t byte)
{
    if(!erase_last(device)) {
        return;
    }

    const termios_t* termios = &device->priv.termios;

    if((termios->c_lflag & ECHO) && !erases_visibly(termios)) {
        echo(device, byte);
    }
}

static void erase_word(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_discipline_state_t* discipline = &device->priv.discipline;
    const tty_ring_t* input = &discipline->input;

    while(input->tail != discipline->line_end && isblank(tty_ring_at(input, input->tail - 1))) {
        (void)erase_last(device);
    }

    while(input->tail != discipline->line_end && !isblank(tty_ring_at(input, input->tail - 1))) {
        (void)erase_last(device);
    }

    if((termios->c_lflag & ECHO) && !erases_visibly(termios)) {
        echo(device, byte);
    }
}

static void kill_line(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_discipline_state_t* discipline = &device->priv.discipline;
    uint32_t lflag = termios->c_lflag;

    if((lflag & (ECHO | ECHOKE | ECHOE)) == (ECHO | ECHOKE | ECHOE)) {
        while(erase_last(device)) {
            // empty
        }

        return;
    }

    discipline->input.tail = discipline->line_end;

    if(lflag & ECHO) {
        echo(device, byte);

        if(lflag & ECHOK) {
            echo(device, '\n');
        }
    }

    discipline->line_column = discipline->column;
}

static void reprint(tty_device_t* restrict device, uint8_t byte)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;
    const tty_ring_t* input = &discipline->input;

    if(0 == (device->priv.termios.c_lflag & ECHO)) {
        return;
    }

    echo(device, byte);
    echo(device, '\n');

    discipline->line_column = discipline->column;

    for(uint32_t position = discipline->line_end; position != input->tail; position += 1) {
        echo(device, tty_ring_at(input, position));
    }
}

// A canonical line keeps a byte free for the delimiter that ends it
static bool input_fits(const tty_device_t* restrict device, uint32_t count)
{
    const tty_discipline_state_t* discipline = &device->priv.discipline;
    const tty_ring_t* input = &discipline->input;

    if(device->priv.termios.c_lflag & ICANON) {
        return tty_ring_room(input) > count && input->tail - discipline->line_end + count < MAX_CANON;
    }

    return tty_ring_room(input) >= count;
}

static void end_raw_data(tty_device_t* restrict device)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;

    if(0 == (device->priv.termios.c_lflag & ICANON)) {
        discipline->line_end = discipline->input.tail;
    }
}

static void put_data(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    uint32_t count = 1;

    close_hardcopy_erase(device);

    if((termios->c_iflag & (PARMRK | IGNPAR)) == PARMRK && byte == MARK) {
        count = 2;
    }

    if(!input_fits(device, count)) {
        if((termios->c_lflag & ICANON) && (termios->c_iflag & IMAXBEL)) {
            output_or_drop(device, BELL);
        }

        return;
    }

    if(termios->c_lflag & ECHO) {
        echo(device, byte);
    }

    for(uint32_t i = 0; i < count; ++i) {
        store(device, byte, false);
    }

    end_raw_data(device);
}

// A byte with an error reads as a NUL, or as a mark with PARMRK; neither is echoed
static void put_error(tty_device_t* restrict device, uint8_t byte)
{
    if(0 == (device->priv.termios.c_iflag & PARMRK)) {
        if(input_fits(device, 1)) {
            store(device, 0, false);
        }
    }
    else if(input_fits(device, 3)) {
        store(device, MARK, false);
        store(device, 0, false);
        store(device, byte, false);
    }

    end_raw_data(device);
}

// A NUL delimiter is VEOF: no special character can be NUL, which is _POSIX_VDISABLE
static void end_line(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_discipline_state_t* discipline = &device->priv.discipline;

    close_hardcopy_erase(device);

    if(tty_ring_room(&discipline->input) == 0) {
        return;
    }

    if(byte && ((termios->c_lflag & ECHO) || (byte == '\n' && (termios->c_lflag & ECHONL)))) {
        echo(device, byte);
    }

    store(device, byte, true);

    discipline->line_end = discipline->input.tail;
    discipline->line_column = discipline->column;
}

static bool flow_control(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;

    if(0 == (termios->c_iflag & IXON)) {
        return false;
    }

    bool start = is_special(termios, byte, VSTART);
    bool stop = is_special(termios, byte, VSTOP);

    if(start && stop) {
        tty_output_set_stopped(device, !tty_output_is_stopped(device));
        return true;
    }

    if(stop || start) {
        tty_output_set_stopped(device, stop);
        return true;
    }

    if(termios->c_iflag & IXANY) {
        tty_output_set_stopped(device, false);
    }

    return false;
}

static int signal_of(const termios_t* restrict termios, uint8_t byte)
{
    if(is_special(termios, byte, VINTR)) {
        return SIGINT;
    }

    if(is_special(termios, byte, VQUIT)) {
        return SIGQUIT;
    }

    if(is_special(termios, byte, VSUSP)) {
        return SIGTSTP;
    }

    if(is_special(termios, byte, VSTATUS)) {
        return SIGINFO;
    }

    return 0;
}

static bool signal_character(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;

    if(0 == (termios->c_lflag & ISIG)) {
        return false;
    }

    int sig = signal_of(termios, byte);

    if(sig == 0) {
        return false;
    }

    // A status request interrupts nothing, so it keeps what was typed and is still to print
    if(sig != SIGINFO && 0 == (termios->c_lflag & NOFLSH)) {
        tty_discipline_flush_input(device);
        tty_output_flush(device);
    }

    if(termios->c_lflag & ECHO) {
        echo(device, byte);
    }

    tty_queue_signal(device, sig);

    return true;
}

static void receive_canonical(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_discipline_state_t* discipline = &device->priv.discipline;
    uint32_t lflag = termios->c_lflag;

    if(discipline->escaped) {
        discipline->escaped = false;

        uint8_t unescaped = xcase_unescape(byte);

        if(islower(byte)) {
            byte = toupper(byte);
        }
        else if(unescaped) {
            byte = unescaped;
        }

        put_data(device, byte);
        return;
    }

    if(is_special(termios, byte, VERASE)) {
        erase(device, byte);
        return;
    }

    if(is_special(termios, byte, VKILL)) {
        kill_line(device, byte);
        return;
    }

    if(lflag & IEXTEN) {
        if(is_special(termios, byte, VWERASE)) {
            erase_word(device, byte);
            return;
        }

        if(is_special(termios, byte, VREPRINT)) {
            reprint(device, byte);
            return;
        }

        if(is_special(termios, byte, VLNEXT)) {
            discipline->literal_next = true;
            return;
        }
    }

    if(is_special(termios, byte, VEOF)) {
        end_line(device, 0);
        return;
    }

    if(byte == '\n' || is_special(termios, byte, VEOL) || ((lflag & IEXTEN) && is_special(termios, byte, VEOL2))) {
        end_line(device, byte);
        return;
    }

    if((lflag & XCASE) && byte == '\\') {
        discipline->escaped = true;

        if(lflag & ECHO) {
            echo(device, byte);
        }

        return;
    }

    put_data(device, byte);
}

static void receive_byte(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_discipline_state_t* discipline = &device->priv.discipline;
    uint32_t iflag = termios->c_iflag;

    if(iflag & ISTRIP) {
        byte &= 0x7F;
    }

    if(discipline->literal_next) {
        discipline->literal_next = false;
        put_data(device, byte);
        return;
    }

    if(flow_control(device, byte) || signal_character(device, byte)) {
        return;
    }

    if(byte == '\r') {
        if(iflag & IGNCR) {
            return;
        }

        if(iflag & ICRNL) {
            byte = '\n';
        }
    }
    else if(byte == '\n' && (iflag & INLCR)) {
        byte = '\r';
    }

    if(iflag & IUCLC) {
        byte = tolower(byte);
    }

    if(0 == (termios->c_lflag & ICANON)) {
        put_data(device, byte);
        return;
    }

    receive_canonical(device, byte);
}

// BRKINT flushes and sends SIGINT whether
// ISIG is set or not, as POSIX has it
static void receive_break(tty_device_t* restrict device)
{
    uint32_t iflag = device->priv.termios.c_iflag;

    if(iflag & IGNBRK) {
        return;
    }

    if(iflag & BRKINT) {
        tty_discipline_flush_input(device);
        tty_output_flush(device);
        tty_queue_signal(device, SIGINT);
        return;
    }

    put_error(device, 0);
}

// A parity or framing error; without INPCK the byte goes in as it came
static void receive_bad_byte(tty_device_t* restrict device, uint8_t byte)
{
    uint32_t iflag = device->priv.termios.c_iflag;

    if(0 == (iflag & INPCK)) {
        receive_byte(device, byte);
        return;
    }

    if(0 == (iflag & IGNPAR)) {
        put_error(device, byte);
    }
}

static void send_flow_character(tty_device_t* restrict device, unsigned index)
{
    uint8_t special = device->priv.termios.c_cc[index];

    if(special != _POSIX_VDISABLE) {
        tty_output_put_ahead(device, special);
    }
}

// A canonical input with no line done has no reader to make room, so
// it isn't stopped; the line is cut off at MAX_CANON instead
static void update_input_flow(tty_device_t* restrict device)
{
    const termios_t* termios = &device->priv.termios;
    tty_discipline_state_t* discipline = &device->priv.discipline;
    uint32_t room = tty_ring_room(&discipline->input);

    if(discipline->input_stopped) {
        if(room >= START_ROOM || 0 == (termios->c_iflag & IXOFF)) {
            discipline->input_stopped = false;
            send_flow_character(device, VSTART);
        }

        return;
    }

    if((termios->c_iflag & IXOFF) && room < STOP_ROOM && tty_discipline_readable(device)) {
        discipline->input_stopped = true;
        send_flow_character(device, VSTOP);
    }
}

int tty_discipline_init(tty_device_t* restrict device)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;

    memset(discipline, 0, sizeof(*discipline));
    discipline->input.data = pslab_alloc_virt();

    if(discipline->input.data == nullptr) {
        return -ENOMEM;
    }

    return 0;
}

void tty_discipline_deinit(tty_device_t* restrict device)
{
    pslab_free_virt(device->priv.discipline.input.data);
}

void tty_discipline_reset(tty_device_t* restrict device)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;
    tty_ring_t input = discipline->input;

    memset(discipline, 0, sizeof(*discipline));

    discipline->input.data = input.data;
    discipline->input.head = input.tail;
    discipline->input.tail = input.tail;
    discipline->line_end = input.tail;
}

void tty_discipline_flush_input(tty_device_t* restrict device)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;
    tty_ring_t* input = &discipline->input;

    input->head = input->tail;
    discipline->line_end = input->tail;
    discipline->line_column = discipline->column;
    discipline->literal_next = false;
    discipline->escaped = false;
    discipline->hardcopy_erasing = false;

    update_input_flow(device);
}

void tty_discipline_receive(tty_device_t* restrict device, uint8_t byte, unsigned error)
{
    if(error == TTY_ERROR_NONE) {
        receive_byte(device, byte);
    }
    else if(error == TTY_ERROR_BREAK) {
        receive_break(device);
    }
    else {
        receive_bad_byte(device, byte);
    }

    update_input_flow(device);
}

bool tty_discipline_output(tty_device_t* restrict device, uint8_t byte)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;
    uint8_t expanded[TTY_EXPANSION_MAX];
    uint32_t column = discipline->column;
    size_t length = expand(&device->priv.termios, byte, expanded, &column);

    if(!tty_output_put(device, expanded, length)) {
        return false;
    }

    discipline->column = column;

    return true;
}

size_t tty_discipline_readable(const tty_device_t* restrict device)
{
    const tty_discipline_state_t* discipline = &device->priv.discipline;
    return discipline->line_end - discipline->input.head;
}

size_t tty_discipline_read(tty_device_t* restrict device, uint8_t* restrict buffer, size_t size, bool* restrict end_of_file)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;
    tty_ring_t* input = &discipline->input;
    bool canonical = device->priv.termios.c_lflag & ICANON;
    size_t copied = 0;

    *end_of_file = false;

    while(copied < size && input->head != discipline->line_end) {
        bool delimiter = canonical && delimiter_at(discipline, input->head);
        uint8_t byte = tty_ring_take(input);

        // VEOF ends the line without being part of
        // it, and on an empty line it is end of file
        if(delimiter && byte == 0) {
            *end_of_file = (copied == 0);
            break;
        }

        buffer[copied] = byte;
        copied += 1;

        if(delimiter) {
            break;
        }
    }

    update_input_flow(device);

    return copied;
}

void tty_discipline_set_termios(tty_device_t* restrict device, const termios_t* restrict old_termios)
{
    tty_discipline_state_t* discipline = &device->priv.discipline;

    if((old_termios->c_lflag & ICANON) && 0 == (device->priv.termios.c_lflag & ICANON)) {
        discipline->line_end = discipline->input.tail;
        discipline->literal_next = false;
        discipline->escaped = false;
    }

    update_input_flow(device);
}
