/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2022, KanOS Contributors */
#include <kan/errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

int strerror_r(int errnum, char *restrict s, size_t n)
{
    switch(errnum) {
        case EDOM:
            kstrncpy(s, "argument out of domain of function", n);
            return 0;
        case EILSEQ:
            kstrncpy(s, "illegal byte sequence", n);
            return 0;
        case ERANGE:
            kstrncpy(s, "result too large", n);
            return 0;
        case E2BIG:
            kstrncpy(s, "argument list too long", n);
            return 0;
        case EACCES:
            kstrncpy(s, "permission denied", n);
            return 0;
        case EADDRINUSE:
            kstrncpy(s, "address in use", n);
            return 0;
        case EADDRNOTAVAIL:
            kstrncpy(s, "address not available", n);
            return 0;
        case EAFNOSUPPORT:
            kstrncpy(s, "address family not supported", n);
            return 0;
        case EAGAIN:
            kstrncpy(s, "resource temporarily unavailable", n);
            return 0;
        case EALREADY:
            kstrncpy(s, "connection already in progress", n);
            return 0;
        case EBADF:
            kstrncpy(s, "bad file descriptor", n);
            return 0;
        case EBADMSG:
            kstrncpy(s, "bad message", n);
            return 0;
        case EBUSY:
            kstrncpy(s, "device or resource busy", n);
            return 0;
        case ECANCELED:
            kstrncpy(s, "operation canceled", n);
            return 0;
        case ECHILD:
            kstrncpy(s, "no child processes", n);
            return 0;
        case ECONNABORTED:
            kstrncpy(s, "connection aborted", n);
            return 0;
        case ECONNREFUSED:
            kstrncpy(s, "connection refused", n);
            return 0;
        case ECONNRESET:
            kstrncpy(s, "connection reset", n);
            return 0;
        case EDEADLK:
            kstrncpy(s, "resource deadlock would occur", n);
            return 0;
        case EDESTADDRREQ:
            kstrncpy(s, "destination address required", n);
            return 0;
        case EDQUOT:
            kstrncpy(s, "disk quota exceeded", n);
            return 0;
        case EEXIST:
            kstrncpy(s, "file exists", n);
            return 0;
        case EFAULT:
            kstrncpy(s, "bad address", n);
            return 0;
        case EFBIG:
            kstrncpy(s, "file too large", n);
            return 0;
        case EHOSTUNREACH:
            kstrncpy(s, "host is unreachable", n);
            return 0;
        case EIDRM:
            kstrncpy(s, "identifier removed", n);
            return 0;
        case EINPROGRESS:
            kstrncpy(s, "operation in progress", n);
            return 0;
        case EINTR:
            kstrncpy(s, "interrupted function call", n);
            return 0;
        case EINVAL:
            kstrncpy(s, "invalid argument", n);
            return 0;
        case EIO:
            kstrncpy(s, "input/output error", n);
            return 0;
        case EISCONN:
            kstrncpy(s, "socket is connected", n);
            return 0;
        case EISDIR:
            kstrncpy(s, "is a directory", n);
            return 0;
        case ELOOP:
            kstrncpy(s, "too many levels of symbolic links", n);
            return 0;
        case EMFILE:
            kstrncpy(s, "too many open files", n);
            return 0;
        case EMLINK:
            kstrncpy(s, "too many links", n);
            return 0;
        case EMSGSIZE:
            kstrncpy(s, "message too long", n);
            return 0;
        case EMULTIHOP:
            kstrncpy(s, "multihop attempted", n);
            return 0;
        case ENAMETOOLONG:
            kstrncpy(s, "filename too long", n);
            return 0;
        case ENETDOWN:
            kstrncpy(s, "network is down", n);
            return 0;
        case ENETRESET:
            kstrncpy(s, "connection aborted by network", n);
            return 0;
        case ENETUNREACH:
            kstrncpy(s, "network unreachable", n);
            return 0;
        case ENFILE:
            kstrncpy(s, "too many files open in system", n);
            return 0;
        case ENOBUFS:
            kstrncpy(s, "no buffer space available", n);
            return 0;
        case ENODATA:
            kstrncpy(s, "no message is available on the stream head read queue", n);
            return 0;
        case ENODEV:
            kstrncpy(s, "no such device", n);
            return 0;
        case ENOENT:
            kstrncpy(s, "no such file or directory", n);
            return 0;
        case ENOEXEC:
            kstrncpy(s, "exec format error", n);
            return 0;
        case ENOLCK:
            kstrncpy(s, "no locks available", n);
            return 0;
        case ENOLINK:
            kstrncpy(s, "link has been severed", n);
            return 0;
        case ENOMEM:
            kstrncpy(s, "not enough space", n);
            return 0;
        case ENOMSG:
            kstrncpy(s, "no message of the desired type", n);
            return 0;
        case ENOPROTOOPT:
            kstrncpy(s, "protocol not available", n);
            return 0;
        case ENOSPC:
            kstrncpy(s, "no space left on device", n);
            return 0;
        case ENOSR:
            kstrncpy(s, "no stream resources", n);
            return 0;
        case ENOSTR:
            kstrncpy(s, "not a stream", n);
            return 0;
        case ENOSYS:
            kstrncpy(s, "function not implemented", n);
            return 0;
        case ENOTCONN:
            kstrncpy(s, "the socket is not connected", n);
            return 0;
        case ENOTDIR:
            kstrncpy(s, "not a directory", n);
            return 0;
        case ENOTEMPTY:
            kstrncpy(s, "directory not empty", n);
            return 0;
        case ENOTSOCK:
            kstrncpy(s, "not a socket", n);
            return 0;
        case ENOTSUP:
            kstrncpy(s, "operation not supported", n);
            return 0;
        case ENOTTY:
            kstrncpy(s, "inappropriate i/o control operation", n);
            return 0;
        case ENXIO:
            kstrncpy(s, "no such device or address", n);
            return 0;
        case EOPNOTSUPP:
            kstrncpy(s, "operation not supported on socket", n);
            return 0;
        case EOVERFLOW:
            kstrncpy(s, "value too large to be stored in data type", n);
            return 0;
        case EPERM:
            kstrncpy(s, "operation not permitted", n);
            return 0;
        case EPIPE:
            kstrncpy(s, "broken pipe", n);
            return 0;
        case EPROTO:
            kstrncpy(s, "protocol error", n);
            return 0;
        case EPROTONOSUPPORT:
            kstrncpy(s, "protocol not supported", n);
            return 0;
        case EPROTOTYPE:
            kstrncpy(s, "protocol wrong type for socket", n);
            return 0;
        case EROFS:
            kstrncpy(s, "read-only file system", n);
            return 0;
        case ESPIPE:
            kstrncpy(s, "invalid seek", n);
            return 0;
        case ESRCH:
            kstrncpy(s, "no such process", n);
            return 0;
        case ESTALE:
            kstrncpy(s, "stale file handle", n);
            return 0;
        case ETIME:
            kstrncpy(s, "stream ioctl() timeout", n);
            return 0;
        case ETIMEDOUT:
            kstrncpy(s, "connection timed out", n);
            return 0;
        case ETXTBSY:
            kstrncpy(s, "text file busy", n);
            return 0;
        case EWOULDBLOCK:
            kstrncpy(s, "operation would block", n);
            return 0;
        case EXDEV:
            kstrncpy(s, "cross-device link", n);
            return 0;
        case 0:
            kstrncpy(s, "no error", n);
            return 0;
        default:
            snprintf(s, n, "unknown errnum (%d)", errnum);
            return EINVAL;
    }
}
