/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/errno.h>
#include <string.h>
#include <sprintf.h>

int strerror_r(int errnum, char *restrict s, size_t n)
{
    switch(errnum) {
        case E2BIG:
            kstrncpy(s, "argument list too long", n);
            return EOK;
        case EACCES:
            kstrncpy(s, "permission denied", n);
            return EOK;
        case EADDRINUSE:
            kstrncpy(s, "address in use", n);
            return EOK;
        case EADDRNOTAVAIL:
            kstrncpy(s, "address not available", n);
            return EOK;
        case EAFNOSUPPORT:
            kstrncpy(s, "address family not supported", n);
            return EOK;
        case EAGAIN:
            kstrncpy(s, "resource temporarily unavailable", n);
            return EOK;
        case EALREADY:
            kstrncpy(s, "connection already in progress", n);
            return EOK;
        case EBADF:
            kstrncpy(s, "bad file descriptor", n);
            return EOK;
        case EBADMSG:
            kstrncpy(s, "bad message", n);
            return EOK;
        case EBUSY:
            kstrncpy(s, "device or resource busy", n);
            return EOK;
        case ECANCELED:
            kstrncpy(s, "operation canceled", n);
            return EOK;
        case ECHILD:
            kstrncpy(s, "no child processes", n);
            return EOK;
        case ECONNABORTED:
            kstrncpy(s, "connection aborted", n);
            return EOK;
        case ECONNREFUSED:
            kstrncpy(s, "connection refused", n);
            return EOK;
        case ECONNRESET:
            kstrncpy(s, "connection reset", n);
            return EOK;
        case EDEADLK:
            kstrncpy(s, "resource deadlock would occur", n);
            return EOK;
        case EDESTADDRREQ:
            kstrncpy(s, "destination address required", n);
            return EOK;
        case EDOM:
            kstrncpy(s, "argument out of domain of function", n);
            return EOK;
        case EDQUOT:
            kstrncpy(s, "disk quota exceeded", n);
            return EOK;
        case EEXIST:
            kstrncpy(s, "file exists", n);
            return EOK;
        case EFAULT:
            kstrncpy(s, "bad address", n);
            return EOK;
        case EFBIG:
            kstrncpy(s, "file too large", n);
            return EOK;
        case EHOSTUNREACH:
            kstrncpy(s, "host is unreachable", n);
            return EOK;
        case EIDRM:
            kstrncpy(s, "identifier removed", n);
            return EOK;
        case EILSEQ:
            kstrncpy(s, "illegal byte sequence", n);
            return EOK;
        case EINPROGRESS:
            kstrncpy(s, "operation in progress", n);
            return EOK;
        case EINTR:
            kstrncpy(s, "interrupted function call", n);
            return EOK;
        case EINVAL:
            kstrncpy(s, "invalid argument", n);
            return EOK;
        case EIO:
            kstrncpy(s, "input/output error", n);
            return EOK;
        case EISCONN:
            kstrncpy(s, "socket is connected", n);
            return EOK;
        case EISDIR:
            kstrncpy(s, "is a directory", n);
            return EOK;
        case ELOOP:
            kstrncpy(s, "too many levels of symbolic links", n);
            return EOK;
        case EMFILE:
            kstrncpy(s, "too many open files", n);
            return EOK;
        case EMLINK:
            kstrncpy(s, "too many links", n);
            return EOK;
        case EMSGSIZE:
            kstrncpy(s, "message too long", n);
            return EOK;
        case EMULTIHOP:
            kstrncpy(s, "multihop attempted", n);
            return EOK;
        case ENAMETOOLONG:
            kstrncpy(s, "filename too long", n);
            return EOK;
        case ENETDOWN:
            kstrncpy(s, "network is down", n);
            return EOK;
        case ENETRESET:
            kstrncpy(s, "connection aborted by network", n);
            return EOK;
        case ENETUNREACH:
            kstrncpy(s, "network unreachable", n);
            return EOK;
        case ENFILE:
            kstrncpy(s, "too many files open in system", n);
            return EOK;
        case ENOBUFS:
            kstrncpy(s, "no buffer space available", n);
            return EOK;
        case ENODATA:
            kstrncpy(s, "no message is available on the stream head read queue", n);
            return EOK;
        case ENODEV:
            kstrncpy(s, "no such device", n);
            return EOK;
        case ENOENT:
            kstrncpy(s, "no such file or directory", n);
            return EOK;
        case ENOEXEC:
            kstrncpy(s, "exec format error", n);
            return EOK;
        case ENOLCK:
            kstrncpy(s, "no locks available", n);
            return EOK;
        case ENOLINK:
            kstrncpy(s, "link has been severed", n);
            return EOK;
        case ENOMEM:
            kstrncpy(s, "not enough space", n);
            return EOK;
        case ENOMSG:
            kstrncpy(s, "no message of the desired type", n);
            return EOK;
        case ENOPROTOOPT:
            kstrncpy(s, "protocol not available", n);
            return EOK;
        case ENOSPC:
            kstrncpy(s, "no space left on device", n);
            return EOK;
        case ENOSR:
            kstrncpy(s, "no stream resources", n);
            return EOK;
        case ENOSTR:
            kstrncpy(s, "not a stream", n);
            return EOK;
        case ENOSYS:
            kstrncpy(s, "function not implemented", n);
            return EOK;
        case ENOTCONN:
            kstrncpy(s, "the socket is not connected", n);
            return EOK;
        case ENOTDIR:
            kstrncpy(s, "not a directory", n);
            return EOK;
        case ENOTEMPTY:
            kstrncpy(s, "directory not empty", n);
            return EOK;
        case ENOTSOCK:
            kstrncpy(s, "not a socket", n);
            return EOK;
        case ENOTSUP:
            kstrncpy(s, "operation not supported", n);
            return EOK;
        case ENOTTY:
            kstrncpy(s, "inappropriate i/o control operation", n);
            return EOK;
        case ENXIO:
            kstrncpy(s, "no such device or address", n);
            return EOK;
        case EOPNOTSUPP:
            kstrncpy(s, "operation not supported on socket", n);
            return EOK;
        case EOVERFLOW:
            kstrncpy(s, "value too large to be stored in data type", n);
            return EOK;
        case EPERM:
            kstrncpy(s, "operation not permitted", n);
            return EOK;
        case EPIPE:
            kstrncpy(s, "broken pipe", n);
            return EOK;
        case EPROTO:
            kstrncpy(s, "protocol error", n);
            return EOK;
        case EPROTONOSUPPORT:
            kstrncpy(s, "protocol not supported", n);
            return EOK;
        case EPROTOTYPE:
            kstrncpy(s, "protocol wrong type for socket", n);
            return EOK;
        case ERANGE:
            kstrncpy(s, "result too large", n);
            return EOK;
        case EROFS:
            kstrncpy(s, "read-only file system", n);
            return EOK;
        case ESPIPE:
            kstrncpy(s, "invalid seek", n);
            return EOK;
        case ESRCH:
            kstrncpy(s, "no such process", n);
            return EOK;
        case ESTALE:
            kstrncpy(s, "stale file handle", n);
            return EOK;
        case ETIME:
            kstrncpy(s, "stream ioctl() timeout", n);
            return EOK;
        case ETIMEDOUT:
            kstrncpy(s, "connection timed out", n);
            return EOK;
        case ETXTBSY:
            kstrncpy(s, "text file busy", n);
            return EOK;
        case EWOULDBLOCK:
            kstrncpy(s, "operation would block", n);
            return EOK;
        case EXDEV:
            kstrncpy(s, "cross-device link", n);
            return EOK;
        case 0:
            kstrncpy(s, "no error", n);
            return EOK;
        default:
            snprintf(s, n, "unknown errnum (%d)", errnum);
            return EINVAL;
    }
}
