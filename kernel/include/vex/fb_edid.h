#pragma once

#include <vex/compiler.h>
#include <vex/fb.h>
#include <vex/types.h>

// VESA E-EDID 1.4, the base block only

#define EDID_BLOCK_SIZE 128

#define EDID_HEADER              0x00 // 00 FF FF FF FF FF FF 00
#define EDID_ESTABLISHED_TIMINGS 0x23 // Three bytes of bits, one per fixed mode
#define EDID_STANDARD_TIMINGS    0x26 // Eight two-byte entries
#define EDID_DESCRIPTORS         0x36 // Four 18-byte descriptors
#define EDID_CHECKSUM            0x7F // Makes the block sum to zero

#define EDID_STANDARD_TIMING_COUNT 8
#define EDID_STANDARD_TIMING_SIZE  2
#define EDID_DESCRIPTOR_COUNT      4
#define EDID_DESCRIPTOR_SIZE       18

/// Collects the resolutions the display advertises, the preferred one first with VEXFB_MODE_PREFERRED,
/// each once; bpp is left zero, as EDID doesn't say
/// @return How many it stored, which is zero when the block isn't EDID
size_t fb_edid_parse_modes(const uint8_t* restrict block, vexfb_mode_t* restrict modes, size_t capacity);
