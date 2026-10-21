#pragma once

#include <usr/vex/termios.h>

#define NCCS            __k_NCCS
#define _POSIX_VDISABLE __k_POSIX_VDISABLE

#define VINTR    __k_VINTR
#define VQUIT    __k_VQUIT
#define VERASE   __k_VERASE
#define VKILL    __k_VKILL
#define VEOF     __k_VEOF
#define VEOL     __k_VEOL
#define VEOL2    __k_VEOL2
#define VSTART   __k_VSTART
#define VSTOP    __k_VSTOP
#define VSUSP    __k_VSUSP
#define VREPRINT __k_VREPRINT
#define VWERASE  __k_VWERASE
#define VLNEXT   __k_VLNEXT
#define VDISCARD __k_VDISCARD
#define VMIN     __k_VMIN
#define VTIME    __k_VTIME
#define VSTATUS  __k_VSTATUS

#define IGNBRK  __k_IGNBRK
#define BRKINT  __k_BRKINT
#define IGNPAR  __k_IGNPAR
#define PARMRK  __k_PARMRK
#define INPCK   __k_INPCK
#define ISTRIP  __k_ISTRIP
#define INLCR   __k_INLCR
#define IGNCR   __k_IGNCR
#define ICRNL   __k_ICRNL
#define IUCLC   __k_IUCLC
#define IXON    __k_IXON
#define IXANY   __k_IXANY
#define IXOFF   __k_IXOFF
#define IMAXBEL __k_IMAXBEL
#define IUTF8   __k_IUTF8

#define OPOST  __k_OPOST
#define OLCUC  __k_OLCUC
#define ONLCR  __k_ONLCR
#define OCRNL  __k_OCRNL
#define ONOCR  __k_ONOCR
#define ONLRET __k_ONLRET
#define OFILL  __k_OFILL
#define OFDEL  __k_OFDEL
#define NLDLY  __k_NLDLY
#define NL0    __k_NL0
#define NL1    __k_NL1
#define CRDLY  __k_CRDLY
#define CR0    __k_CR0
#define CR1    __k_CR1
#define CR2    __k_CR2
#define CR3    __k_CR3
#define TABDLY __k_TABDLY
#define TAB0   __k_TAB0
#define TAB1   __k_TAB1
#define TAB2   __k_TAB2
#define TAB3   __k_TAB3
#define BSDLY  __k_BSDLY
#define BS0    __k_BS0
#define BS1    __k_BS1
#define VTDLY  __k_VTDLY
#define VT0    __k_VT0
#define VT1    __k_VT1
#define FFDLY  __k_FFDLY
#define FF0    __k_FF0
#define FF1    __k_FF1

#define CSIZE   __k_CSIZE
#define CS5     __k_CS5
#define CS6     __k_CS6
#define CS7     __k_CS7
#define CS8     __k_CS8
#define CSTOPB  __k_CSTOPB
#define CREAD   __k_CREAD
#define PARENB  __k_PARENB
#define PARODD  __k_PARODD
#define HUPCL   __k_HUPCL
#define CLOCAL  __k_CLOCAL
#define CRTSCTS __k_CRTSCTS

#define ISIG       __k_ISIG
#define ICANON     __k_ICANON
#define XCASE      __k_XCASE
#define ECHO       __k_ECHO
#define ECHOE      __k_ECHOE
#define ECHOK      __k_ECHOK
#define ECHONL     __k_ECHONL
#define ECHOPRT    __k_ECHOPRT
#define ECHOCTL    __k_ECHOCTL
#define ECHOKE     __k_ECHOKE
#define NOFLSH     __k_NOFLSH
#define TOSTOP     __k_TOSTOP
#define IEXTEN     __k_IEXTEN
#define NOKERNINFO __k_NOKERNINFO

#define TCIFLUSH  __k_TCIFLUSH
#define TCOFLUSH  __k_TCOFLUSH
#define TCIOFLUSH __k_TCIOFLUSH

#define TCOOFF __k_TCOOFF
#define TCOON  __k_TCOON
#define TCIOFF __k_TCIOFF
#define TCION  __k_TCION

#define TCGETS      __k_TCGETS
#define TCSETS      __k_TCSETS
#define TCSETSW     __k_TCSETSW
#define TCSETSF     __k_TCSETSF
#define TIOCGWINSZ  __k_TIOCGWINSZ
#define TIOCSWINSZ  __k_TIOCSWINSZ
#define TIOCGPGRP   __k_TIOCGPGRP
#define TIOCSPGRP   __k_TIOCSPGRP
#define TIOCGSID    __k_TIOCGSID
#define TIOCSCTTY   __k_TIOCSCTTY
#define TIOCNOTTY   __k_TIOCNOTTY
#define TCFLSH      __k_TCFLSH
#define TCXONC      __k_TCXONC
#define TCDRAIN     __k_TCDRAIN
#define TCSENDBREAK __k_TCSENDBREAK
#define TIOCEXCL    __k_TIOCEXCL
#define TIOCNXCL    __k_TIOCNXCL
#define TIOCOUTQ    __k_TIOCOUTQ

typedef struct __k_termios termios_t;
typedef struct __k_winsize winsize_t;

static_assert(sizeof(termios_t) == 44);
static_assert(sizeof(winsize_t) == 8);
