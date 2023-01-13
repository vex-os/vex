/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/errno.h>
#include <string.h>
#include <sprintf.h>

int strerror_r(int errnum, char *restrict s, size_t n)
{
    switch(errnum) {
        case E2BIG:
            strlcpy(s, "argument list too long", n);
            return 0;
        case EACCES:
            strlcpy(s, "permission denied", n);
            return 0;
        case EADDRINUSE:
            strlcpy(s, "address in use", n);
            return 0;
        case EADDRNOTAVAIL:
            strlcpy(s, "address not available", n);
            return 0;
        case EAFNOSUPPORT:
            strlcpy(s, "address family not supported", n);
            return 0;
        case EAGAIN:
            strlcpy(s, "resource temporarily unavailable", n);
            return 0;
        case EALREADY:
            strlcpy(s, "connection already in progress", n);
            return 0;
        case EBADF:
            strlcpy(s, "bad file descriptor", n);
            return 0;
        case EBADMSG:
            strlcpy(s, "bad message", n);
            return 0;
        case EBUSY:
            strlcpy(s, "device or resource busy", n);
            return 0;
        case ECANCELED:
            strlcpy(s, "operation canceled", n);
            return 0;
        case ECHILD:
            strlcpy(s, "no child processes", n);
            return 0;
        case ECONNABORTED:
            strlcpy(s, "connection aborted", n);
            return 0;
        case ECONNREFUSED:
            strlcpy(s, "connection refused", n);
            return 0;
        case ECONNRESET:
            strlcpy(s, "connection reset", n);
            return 0;
        case EDEADLK:
            strlcpy(s, "resource deadlock would occur", n);
            return 0;
        case EDESTADDRREQ:
            strlcpy(s, "destination address required", n);
            return 0;
        case EDOM:
            strlcpy(s, "argument out of domain of function", n);
            return 0;
        case EDQUOT:
            strlcpy(s, "disk quota exceeded", n);
            return 0;
        case EEXIST:
            strlcpy(s, "file exists", n);
            return 0;
        case EFAULT:
            strlcpy(s, "bad address", n);
            return 0;
        case EFBIG:
            strlcpy(s, "file too large", n);
            return 0;
        case EHOSTUNREACH:
            strlcpy(s, "host is unreachable", n);
            return 0;
        case EIDRM:
            strlcpy(s, "identifier removed", n);
            return 0;
        case EILSEQ:
            strlcpy(s, "illegal byte sequence", n);
            return 0;
        case EINPROGRESS:
            strlcpy(s, "operation in progress", n);
            return 0;
        case EINTR:
            strlcpy(s, "interrupted function call", n);
            return 0;
        case EINVAL:
            strlcpy(s, "invalid argument", n);
            return 0;
        case EIO:
            strlcpy(s, "input/output error", n);
            return 0;
        case EISCONN:
            strlcpy(s, "socket is connected", n);
            return 0;
        case EISDIR:
            strlcpy(s, "is a directory", n);
            return 0;
        case ELOOP:
            strlcpy(s, "too many levels of symbolic links", n);
            return 0;
        case EMFILE:
            strlcpy(s, "too many open files", n);
            return 0;
        case EMLINK:
            strlcpy(s, "too many links", n);
            return 0;
        case EMSGSIZE:
            strlcpy(s, "message too long", n);
            return 0;
        case EMULTIHOP:
            strlcpy(s, "multihop attempted", n);
            return 0;
        case ENAMETOOLONG:
            strlcpy(s, "filename too long", n);
            return 0;
        case ENETDOWN:
            strlcpy(s, "network is down", n);
            return 0;
        case ENETRESET:
            strlcpy(s, "connection aborted by network", n);
            return 0;
        case ENETUNREACH:
            strlcpy(s, "network unreachable", n);
            return 0;
        case ENFILE:
            strlcpy(s, "too many files open in system", n);
            return 0;
        case ENOBUFS:
            strlcpy(s, "no buffer space available", n);
            return 0;
        case ENODATA:
            strlcpy(s, "no message is available on the stream head read queue", n);
            return 0;
        case ENODEV:
            strlcpy(s, "no such device", n);
            return 0;
        case ENOENT:
            strlcpy(s, "no such file or directory", n);
            return 0;
        case ENOEXEC:
            strlcpy(s, "exec format error", n);
            return 0;
        case ENOLCK:
            strlcpy(s, "no locks available", n);
            return 0;
        case ENOLINK:
            strlcpy(s, "link has been severed", n);
            return 0;
        case ENOMEM:
            strlcpy(s, "not enough space", n);
            return 0;
        case ENOMSG:
            strlcpy(s, "no message of the desired type", n);
            return 0;
        case ENOPROTOOPT:
            strlcpy(s, "protocol not available", n);
            return 0;
        case ENOSPC:
            strlcpy(s, "no space left on device", n);
            return 0;
        case ENOSR:
            strlcpy(s, "no stream resources", n);
            return 0;
        case ENOSTR:
            strlcpy(s, "not a stream", n);
            return 0;
        case ENOSYS:
            strlcpy(s, "function not implemented", n);
            return 0;
        case ENOTCONN:
            strlcpy(s, "the socket is not connected", n);
            return 0;
        case ENOTDIR:
            strlcpy(s, "not a directory", n);
            return 0;
        case ENOTEMPTY:
            strlcpy(s, "directory not empty", n);
            return 0;
        case ENOTSOCK:
            strlcpy(s, "not a socket", n);
            return 0;
        case ENOTSUP:
            strlcpy(s, "operation not supported", n);
            return 0;
        case ENOTTY:
            strlcpy(s, "inappropriate i/o control operation", n);
            return 0;
        case ENXIO:
            strlcpy(s, "no such device or address", n);
            return 0;
        case EOPNOTSUPP:
            strlcpy(s, "operation not supported on socket", n);
            return 0;
        case EOVERFLOW:
            strlcpy(s, "value too large to be stored in data type", n);
            return 0;
        case EPERM:
            strlcpy(s, "operation not permitted", n);
            return 0;
        case EPIPE:
            strlcpy(s, "broken pipe", n);
            return 0;
        case EPROTO:
            strlcpy(s, "protocol error", n);
            return 0;
        case EPROTONOSUPPORT:
            strlcpy(s, "protocol not supported", n);
            return 0;
        case EPROTOTYPE:
            strlcpy(s, "protocol wrong type for socket", n);
            return 0;
        case ERANGE:
            strlcpy(s, "result too large", n);
            return 0;
        case EROFS:
            strlcpy(s, "read-only file system", n);
            return 0;
        case ESPIPE:
            strlcpy(s, "invalid seek", n);
            return 0;
        case ESRCH:
            strlcpy(s, "no such process", n);
            return 0;
        case ESTALE:
            strlcpy(s, "stale file handle", n);
            return 0;
        case ETIME:
            strlcpy(s, "stream ioctl() timeout", n);
            return 0;
        case ETIMEDOUT:
            strlcpy(s, "connection timed out", n);
            return 0;
        case ETXTBSY:
            strlcpy(s, "text file busy", n);
            return 0;
        case EWOULDBLOCK:
            strlcpy(s, "operation would block", n);
            return 0;
        case EXDEV:
            strlcpy(s, "cross-device link", n);
            return 0;
        case 0:
            strlcpy(s, "no error", n);
            return 0;
        default:
            snprintf(s, n, "unknown errnum (%d)", errnum);
            return EINVAL;
    }
}
