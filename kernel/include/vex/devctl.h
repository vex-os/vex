#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct device_s device_t;

#define DEVCTL_LINE_MAX 512 // With the newline

typedef struct devctl_line_s devctl_line_t;

// A line of /dev/devctl: an event name and key=value fields, which is a command of POSIX sh
struct devctl_line_s {
    char text[DEVCTL_LINE_MAX];
    size_t length; // Without the newline, which devctl adds
};

/// Adds a key=value field to the line. A byte of the value outside [A-Za-z0-9_.:/,+-] becomes %XX
/// @note A field that doesn't fit is left out whole
void devctl_put(devctl_line_t* restrict line, const char* restrict key, const char* restrict fmt, ...) __printflike(3, 4);

/// Tells the readers of /dev/devctl that an interface of a node appeared
/// @param kind block or char
void devctl_interface_added(const device_t* restrict device, const char* restrict kind, const char* restrict name);

void devctl_interface_removed(const device_t* restrict device, const char* restrict name);
