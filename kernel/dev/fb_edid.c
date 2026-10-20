#include <vex/fb_edid.h>
#include <vex/string.h>

typedef struct established_mode_s established_mode_t;

struct established_mode_s {
    uint16_t width;
    uint16_t height;
};

// Bit 7 of the first byte down to bit 0 of
// the third; the interlaced 1024x768 is left out
static const established_mode_t established_modes[24] = {
    { 720, 400 },
    { 720, 400 },
    { 640, 480 },
    { 640, 480 },
    { 640, 480 },
    { 640, 480 },
    { 800, 600 },
    { 800, 600 },
    { 800, 600 },
    { 800, 600 },
    { 832, 624 },
    { 0, 0 },
    { 1024, 768 },
    { 1024, 768 },
    { 1024, 768 },
    { 1280, 1024 },
    { 1152, 870 },
};

static bool is_edid(const uint8_t* restrict block)
{
    static const uint8_t header[8] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

    if(memcmp(&block[EDID_HEADER], header, sizeof(header))) {
        return false;
    }

    uint8_t sum = 0;

    for(size_t i = 0; i < EDID_BLOCK_SIZE; ++i) {
        sum += block[i];
    }

    return sum == 0;
}

static size_t add_mode(vexfb_mode_t* restrict modes, size_t count, size_t capacity, uint32_t width, uint32_t height, uint32_t flags)
{
    if(width == 0 || height == 0) {
        return count;
    }

    for(size_t i = 0; i < count; ++i) {
        if(modes[i].width == width && modes[i].height == height) {
            return count;
        }
    }

    if(count >= capacity) {
        return count;
    }

    modes[count].width = width;
    modes[count].height = height;
    modes[count].bpp = 0;
    modes[count].flags = flags;

    return count + 1;
}

// The height comes from the aspect ratio, where 00 means 16:10 since EDID 1.3
static size_t add_standard_timing(vexfb_mode_t* restrict modes, size_t count, size_t capacity, const uint8_t* restrict timing)
{
    if(timing[0] == 0x01 && timing[1] == 0x01) {
        return count;
    }

    uint32_t width = (timing[0] + 31) * 8;
    uint32_t height;

    switch(timing[1] >> 6) {
        case 0:
            height = width * 10 / 16;
            break;
        case 1:
            height = width * 3 / 4;
            break;
        case 2:
            height = width * 4 / 5;
            break;
        default:
            height = width * 9 / 16;
            break;
    }

    return add_mode(modes, count, capacity, width, height, 0);
}

size_t fb_edid_parse_modes(const uint8_t* restrict block, vexfb_mode_t* restrict modes, size_t capacity)
{
    if(!is_edid(block)) {
        return 0;
    }

    size_t count = 0;

    // A descriptor with a zero pixel clock
    // carries a name or limits, not a timing
    for(size_t i = 0; i < EDID_DESCRIPTOR_COUNT; ++i) {
        const uint8_t* descriptor = &block[EDID_DESCRIPTORS + i * EDID_DESCRIPTOR_SIZE];

        if(descriptor[0] == 0 && descriptor[1] == 0) {
            continue;
        }

        uint32_t width = descriptor[2] | ((uint32_t)(descriptor[4] & 0xF0) << 4);
        uint32_t height = descriptor[5] | ((uint32_t)(descriptor[7] & 0xF0) << 4);

        count = add_mode(modes, count, capacity, width, height, count == 0 ? VEXFB_MODE_PREFERRED : 0);
    }

    for(size_t i = 0; i < EDID_STANDARD_TIMING_COUNT; ++i) {
        count = add_standard_timing(modes, count, capacity, &block[EDID_STANDARD_TIMINGS + i * EDID_STANDARD_TIMING_SIZE]);
    }

    for(size_t bit = 0; bit < 24; ++bit) {
        if(block[EDID_ESTABLISHED_TIMINGS + bit / 8] & (0x80 >> (bit % 8))) {
            count = add_mode(modes, count, capacity, established_modes[bit].width, established_modes[bit].height, 0);
        }
    }

    return count;
}
