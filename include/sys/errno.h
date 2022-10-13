/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_SYS_ERRNO_H__
#define __INCLUDE_SYS_ERRNO_H__

#define EOK             0x00 /* no error */
#define E2BIG           0x01 /* argument list too long */
#define EACCES          0x02 /* permission denied */
#define EADDRINUSE      0x03 /* address in use */
#define EADDRNOTAVAIL   0x04 /* address not available */
#define EAFNOSUPPORT    0x05 /* address family not supported */
#define EAGAIN          0x06 /* resource unavailable, try again */
#define EALREADY        0x07 /* connection already in progress */
#define EBADF           0x08 /* bad file descriptor */
#define EBADMSG         0x09 /* bad message */
#define EBUSY           0x0A /* device or resource busy */
#define ECANCELED       0x0B /* operation canceled */
#define ECHILD          0x0C /* no child processes */
#define ECONNABORTED    0x0D /* connection aborted */
#define ECONNREFUSED    0x0E /* connection refused */
#define ECONNRESET      0x0F /* connection reset */
#define EDEADLK         0x10 /* resource deadlock would occur */
#define EDESTADDRREQ    0x11 /* destination address required */
#define EDOM            0x12 /* mathematics argument out of domain of function */
#define EDQUOT          0x13 /* reserved */
#define EEXIST          0x14 /* file exists */
#define EFAULT          0x15 /* bad address */
#define EFBIG           0x16 /* file too large */
#define EHOSTUNREACH    0x17 /* host is unreachable */
#define EIDRM           0x18 /* identifier removed */
#define EILSEQ          0x19 /* illegal byte sequence */
#define EINPROGRESS     0x1A /* operation in progress */
#define EINTR           0x1B /* interrupted function */
#define EINVAL          0x1C /* invalid argument */
#define EIO             0x1D /* i/o error */
#define EISCONN         0x1E /* socket is connected */
#define EISDIR          0x1F /* is a directory */
#define ELOOP           0x20 /* too many levels of symbolic links */
#define EMFILE          0x21 /* file descriptor value too large */
#define EMLINK          0x22 /* too many links */
#define EMSGSIZE        0x23 /* message too large */
#define EMULTIHOP       0x24 /* reserved */
#define ENAMETOOLONG    0x25 /* filename too long */
#define ENETDOWN        0x26 /* network is down */
#define ENETRESET       0x27 /* connection aborted by network */
#define ENETUNREACH     0x28 /* network unreachable */
#define ENFILE          0x29 /* too many files open in system */
#define ENOBUFS         0x2A /* no buffer space available */
#define ENODATA         0x2B /* no message is available on the stream head read queue */
#define ENODEV          0x2C /* no such device */
#define ENOENT          0x2D /* no such file or directory */
#define ENOEXEC         0x2E /* executable file format error */
#define ENOLCK          0x2F /* no locks available */
#define ENOLINK         0x30 /* reserved */
#define ENOMEM          0x31 /* not enough space */
#define ENOMSG          0x32 /* no message of the desired type */
#define ENOPROTOOPT     0x33 /* protocol not available */
#define ENOSPC          0x34 /* no space left on device */
#define ENOSR           0x35 /* no stream resources */
#define ENOSTR          0x36 /* not a stream */
#define ENOSYS          0x37 /* functionality not supported */
#define ENOTCONN        0x38 /* the socket is not connected */
#define ENOTDIR         0x39 /* not a directory or a symbolic link to a directory */
#define ENOTEMPTY       0x3A /* directory not empty */
#define ENOTRECOVERABLE 0x3B /* state not recoverable */
#define ENOTSOCK        0x3C /* not a socket */
#define ENOTSUP         0x3D /* not supported */
#define ENOTTY          0x3E /* inappropriate i/o control operation */
#define ENXIO           0x3F /* no such device or address */
#define EOPNOTSUPP      0x40 /* operation not supported on socket */
#define EOVERFLOW       0x41 /* value too large to be stored in data type */
#define EOWNERDEAD      0x42 /* previous owner died */
#define EPERM           0x43 /* operation not permitted */
#define EPIPE           0x44 /* broken pipe */
#define EPROTO          0x45 /* protocol error */
#define EPROTONOSUPPORT 0x46 /* protocol not supported */
#define EPROTOTYPE      0x47 /* protocol wrong type for socket */
#define ERANGE          0x48 /* result too large */
#define EROFS           0x49 /* read-only file system */
#define ESPIPE          0x4A /* invalid seek */
#define ESRCH           0x4B /* no such process */
#define ESTALE          0x4C /* reserved */
#define ETIME           0x4D /* stream ioctl() timeout */
#define ETIMEDOUT       0x4E /* connection timed out */
#define ETXTBSY         0x4F /* text file busy */
#define EWOULDBLOCK     0x50 /* operation would block */
#define EXDEV           0x51 /* cross-device link */
#define ERRNUM_COUNT    0x52

#if defined(__kernel__)
#include <sys/cdefs.h>
const char *kstrerror(int errnum) __weak;
#endif

#endif /* __INCLUDE_SYS_ERRNO_H__ */
