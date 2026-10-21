#ifndef USR_VEX_TERMIOS_H
#define USR_VEX_TERMIOS_H

#include <vex/ioctl.h>
#include <vex/types.h>

#define __k_NCCS           20
#define __k_POSIX_VDISABLE 0x00

#define __k_VINTR    0
#define __k_VQUIT    1
#define __k_VERASE   2
#define __k_VKILL    3
#define __k_VEOF     4
#define __k_VEOL     5
#define __k_VEOL2    6
#define __k_VSTART   7
#define __k_VSTOP    8
#define __k_VSUSP    9
#define __k_VREPRINT 10
#define __k_VWERASE  11
#define __k_VLNEXT   12
#define __k_VDISCARD 13
#define __k_VMIN     14
#define __k_VTIME    15
#define __k_VSTATUS  16

#define __k_IGNBRK  ((__k_uint32_t)(0x0001))
#define __k_BRKINT  ((__k_uint32_t)(0x0002))
#define __k_IGNPAR  ((__k_uint32_t)(0x0004))
#define __k_PARMRK  ((__k_uint32_t)(0x0008))
#define __k_INPCK   ((__k_uint32_t)(0x0010))
#define __k_ISTRIP  ((__k_uint32_t)(0x0020))
#define __k_INLCR   ((__k_uint32_t)(0x0040))
#define __k_IGNCR   ((__k_uint32_t)(0x0080))
#define __k_ICRNL   ((__k_uint32_t)(0x0100))
#define __k_IUCLC   ((__k_uint32_t)(0x0200))
#define __k_IXON    ((__k_uint32_t)(0x0400))
#define __k_IXANY   ((__k_uint32_t)(0x0800))
#define __k_IXOFF   ((__k_uint32_t)(0x1000))
#define __k_IMAXBEL ((__k_uint32_t)(0x2000))
#define __k_IUTF8   ((__k_uint32_t)(0x4000))

#define __k_OPOST  ((__k_uint32_t)(0x0001))
#define __k_OLCUC  ((__k_uint32_t)(0x0002))
#define __k_ONLCR  ((__k_uint32_t)(0x0004))
#define __k_OCRNL  ((__k_uint32_t)(0x0008))
#define __k_ONOCR  ((__k_uint32_t)(0x0010))
#define __k_ONLRET ((__k_uint32_t)(0x0020))
#define __k_OFILL  ((__k_uint32_t)(0x0040))
#define __k_OFDEL  ((__k_uint32_t)(0x0080))
#define __k_NLDLY  ((__k_uint32_t)(0x0100))
#define __k_NL0    ((__k_uint32_t)(0x0000))
#define __k_NL1    ((__k_uint32_t)(0x0100))
#define __k_CRDLY  ((__k_uint32_t)(0x0600))
#define __k_CR0    ((__k_uint32_t)(0x0000))
#define __k_CR1    ((__k_uint32_t)(0x0200))
#define __k_CR2    ((__k_uint32_t)(0x0400))
#define __k_CR3    ((__k_uint32_t)(0x0600))
#define __k_TABDLY ((__k_uint32_t)(0x1800))
#define __k_TAB0   ((__k_uint32_t)(0x0000))
#define __k_TAB1   ((__k_uint32_t)(0x0800))
#define __k_TAB2   ((__k_uint32_t)(0x1000))
#define __k_TAB3   ((__k_uint32_t)(0x1800))
#define __k_BSDLY  ((__k_uint32_t)(0x2000))
#define __k_BS0    ((__k_uint32_t)(0x0000))
#define __k_BS1    ((__k_uint32_t)(0x2000))
#define __k_VTDLY  ((__k_uint32_t)(0x4000))
#define __k_VT0    ((__k_uint32_t)(0x0000))
#define __k_VT1    ((__k_uint32_t)(0x4000))
#define __k_FFDLY  ((__k_uint32_t)(0x8000))
#define __k_FF0    ((__k_uint32_t)(0x0000))
#define __k_FF1    ((__k_uint32_t)(0x8000))

#define __k_CSIZE   ((__k_uint32_t)(0x0003))
#define __k_CS5     ((__k_uint32_t)(0x0000))
#define __k_CS6     ((__k_uint32_t)(0x0001))
#define __k_CS7     ((__k_uint32_t)(0x0002))
#define __k_CS8     ((__k_uint32_t)(0x0003))
#define __k_CSTOPB  ((__k_uint32_t)(0x0004))
#define __k_CREAD   ((__k_uint32_t)(0x0008))
#define __k_PARENB  ((__k_uint32_t)(0x0010))
#define __k_PARODD  ((__k_uint32_t)(0x0020))
#define __k_HUPCL   ((__k_uint32_t)(0x0040))
#define __k_CLOCAL  ((__k_uint32_t)(0x0080))
#define __k_CRTSCTS ((__k_uint32_t)(0x0100))

#define __k_ISIG       ((__k_uint32_t)(0x0001))
#define __k_ICANON     ((__k_uint32_t)(0x0002))
#define __k_XCASE      ((__k_uint32_t)(0x0004))
#define __k_ECHO       ((__k_uint32_t)(0x0008))
#define __k_ECHOE      ((__k_uint32_t)(0x0010))
#define __k_ECHOK      ((__k_uint32_t)(0x0020))
#define __k_ECHONL     ((__k_uint32_t)(0x0040))
#define __k_ECHOPRT    ((__k_uint32_t)(0x0080))
#define __k_ECHOCTL    ((__k_uint32_t)(0x0100))
#define __k_ECHOKE     ((__k_uint32_t)(0x0200))
#define __k_NOFLSH     ((__k_uint32_t)(0x0400))
#define __k_TOSTOP     ((__k_uint32_t)(0x0800))
#define __k_IEXTEN     ((__k_uint32_t)(0x1000))
#define __k_NOKERNINFO ((__k_uint32_t)(0x2000))

#define __k_TCIFLUSH  0
#define __k_TCOFLUSH  1
#define __k_TCIOFLUSH 2

#define __k_TCOOFF 0
#define __k_TCOON  1
#define __k_TCIOFF 2
#define __k_TCION  3

/* Speeds are in bits per second; zero in c_ospeed
 * hangs up, zero in c_ispeed follows c_ospeed */
struct __k_termios {
    __k_uint32_t c_iflag;
    __k_uint32_t c_oflag;
    __k_uint32_t c_cflag;
    __k_uint32_t c_lflag;
    __k_uint8_t c_cc[__k_NCCS];
    __k_uint32_t c_ispeed;
    __k_uint32_t c_ospeed;
};

struct __k_winsize {
    __k_uint16_t ws_row;
    __k_uint16_t ws_col;
    __k_uint16_t ws_xpixel;
    __k_uint16_t ws_ypixel;
};

#define __k_TCGETS      __k_IOCTL_READ(sizeof(struct __k_termios), 'T', 0x01)
#define __k_TCSETS      __k_IOCTL_WRITE(sizeof(struct __k_termios), 'T', 0x02)
#define __k_TCSETSW     __k_IOCTL_WRITE(sizeof(struct __k_termios), 'T', 0x03)
#define __k_TCSETSF     __k_IOCTL_WRITE(sizeof(struct __k_termios), 'T', 0x04)
#define __k_TIOCGWINSZ  __k_IOCTL_READ(sizeof(struct __k_winsize), 'T', 0x05)
#define __k_TIOCSWINSZ  __k_IOCTL_WRITE(sizeof(struct __k_winsize), 'T', 0x06)
#define __k_TIOCGPGRP   __k_IOCTL_READ(sizeof(__k_pid_t), 'T', 0x07)
#define __k_TIOCSPGRP   __k_IOCTL_WRITE(sizeof(__k_pid_t), 'T', 0x08)
#define __k_TIOCGSID    __k_IOCTL_READ(sizeof(__k_pid_t), 'T', 0x09)
#define __k_TIOCSCTTY   __k_IOCTL_WRITE(sizeof(__k_int32_t), 'T', 0x0A)
#define __k_TIOCNOTTY   __k_IOCTL_VOID('T', 0x0B)
#define __k_TCFLSH      __k_IOCTL_WRITE(sizeof(__k_int32_t), 'T', 0x0C)
#define __k_TCXONC      __k_IOCTL_WRITE(sizeof(__k_int32_t), 'T', 0x0D)
#define __k_TCDRAIN     __k_IOCTL_VOID('T', 0x0E)
#define __k_TCSENDBREAK __k_IOCTL_WRITE(sizeof(__k_int32_t), 'T', 0x0F)
#define __k_TIOCEXCL    __k_IOCTL_VOID('T', 0x10)
#define __k_TIOCNXCL    __k_IOCTL_VOID('T', 0x11)
#define __k_TIOCOUTQ    __k_IOCTL_READ(sizeof(__k_int32_t), 'T', 0x12)

#endif /* USR_VEX_TERMIOS_H */
