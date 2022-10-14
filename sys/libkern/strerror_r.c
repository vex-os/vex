/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <errno.h>
#include <string.h>
#include <sprintf.h>

int strerror_r(int errnum, char *restrict s, size_t n)
{
    switch(errnum) {
        case E2BIG:
            strncpy_k(s, "argument list too long", n);
            return 0;
        case EACCES:
            strncpy_k(s, "permission denied", n);
            return 0;
        case EADDRINUSE:
            strncpy_k(s, "address in use", n);
            return 0;
        case EADDRNOTAVAIL:
            strncpy_k(s, "address not available", n);
            return 0;
        case EAFNOSUPPORT:
            strncpy_k(s, "address family not supported", n);
            return 0;
        case EAGAIN:
            strncpy_k(s, "resource unavailable, try again", n);
            return 0;
        case EALREADY:
            strncpy_k(s, "connection already in progress", n);
            return 0;
        case EBADF:
            strncpy_k(s, "bad file descriptor", n);
            return 0;
        case EBADMSG:
            strncpy_k(s, "bad message", n);
            return 0;
        case EBUSY:
            strncpy_k(s, "device or resource busy", n);
            return 0;
        case ECANCELED:
            strncpy_k(s, "operation canceled", n);
            return 0;
        case ECHILD:
            strncpy_k(s, "no child processes", n);
            return 0;
        case ECONNABORTED:
            strncpy_k(s, "connection aborted", n);
            return 0;
        case ECONNREFUSED:
            strncpy_k(s, "connection refused", n);
            return 0;
        case ECONNRESET:
            strncpy_k(s, "connection reset", n);
            return 0;
        case EDEADLK:
            strncpy_k(s, "resource deadlock would occur", n);
            return 0;
        case EDESTADDRREQ:
            strncpy_k(s, "destination address required", n);
            return 0;
        case EDOM:
            strncpy_k(s, "mathematics argument out of domain of function", n);
            return 0;
        case EDQUOT:
            strncpy_k(s, "reserved", n);
            return 0;
        case EEXIST:
            strncpy_k(s, "file exists", n);
            return 0;
        case EFAULT:
            strncpy_k(s, "bad address", n);
            return 0;
        case EFBIG:
            strncpy_k(s, "file too large", n);
            return 0;
        case EHOSTUNREACH:
            strncpy_k(s, "host is unreachable", n);
            return 0;
        case EIDRM:
            strncpy_k(s, "identifier removed", n);
            return 0;
        case EILSEQ:
            strncpy_k(s, "illegal byte sequence", n);
            return 0;
        case EINPROGRESS:
            strncpy_k(s, "operation in progress", n);
            return 0;
        case EINTR:
            strncpy_k(s, "interrupted function", n);
            return 0;
        case EINVAL:
            strncpy_k(s, "invalid argument", n);
            return 0;
        case EIO:
            strncpy_k(s, "i/o error", n);
            return 0;
        case EISCONN:
            strncpy_k(s, "socket is connected", n);
            return 0;
        case EISDIR:
            strncpy_k(s, "is a directory", n);
            return 0;
        case ELOOP:
            strncpy_k(s, "too many levels of symbolic links", n);
            return 0;
        case EMFILE:
            strncpy_k(s, "file descriptor value too large", n);
            return 0;
        case EMLINK:
            strncpy_k(s, "too many links", n);
            return 0;
        case EMSGSIZE:
            strncpy_k(s, "message too large", n);
            return 0;
        case EMULTIHOP:
            strncpy_k(s, "reserved", n);
            return 0;
        case ENAMETOOLONG:
            strncpy_k(s, "filename too long", n);
            return 0;
        case ENETDOWN:
            strncpy_k(s, "network is down", n);
            return 0;
        case ENETRESET:
            strncpy_k(s, "connection aborted by network", n);
            return 0;
        case ENETUNREACH:
            strncpy_k(s, "network unreachable", n);
            return 0;
        case ENFILE:
            strncpy_k(s, "too many files open in system", n);
            return 0;
        case ENOBUFS:
            strncpy_k(s, "no buffer space available", n);
            return 0;
        case ENODATA:
            strncpy_k(s, "no message is available on the stream head read queue", n);
            return 0;
        case ENODEV:
            strncpy_k(s, "no such device", n);
            return 0;
        case ENOENT:
            strncpy_k(s, "no such file or directory", n);
            return 0;
        case ENOEXEC:
            strncpy_k(s, "executable file format error", n);
            return 0;
        case ENOLCK:
            strncpy_k(s, "no locks available", n);
            return 0;
        case ENOLINK:
            strncpy_k(s, "reserved", n);
            return 0;
        case ENOMEM:
            strncpy_k(s, "not enough space", n);
            return 0;
        case ENOMSG:
            strncpy_k(s, "no message of the desired type", n);
            return 0;
        case ENOPROTOOPT:
            strncpy_k(s, "protocol not available", n);
            return 0;
        case ENOSPC:
            strncpy_k(s, "no space left on device", n);
            return 0;
        case ENOSR:
            strncpy_k(s, "no stream resources", n);
            return 0;
        case ENOSTR:
            strncpy_k(s, "not a stream", n);
            return 0;
        case ENOSYS:
            strncpy_k(s, "functionality not supported", n);
            return 0;
        case ENOTCONN:
            strncpy_k(s, "the socket is not connected", n);
            return 0;
        case ENOTDIR:
            strncpy_k(s, "not a directory or a symbolic link to a directory", n);
            return 0;
        case ENOTEMPTY:
            strncpy_k(s, "directory not empty", n);
            return 0;
        case ENOTRECOVERABLE:
            strncpy_k(s, "state not recoverable", n);
            return 0;
        case ENOTSOCK:
            strncpy_k(s, "not a socket", n);
            return 0;
        case ENOTSUP:
            strncpy_k(s, "not supported", n);
            return 0;
        case ENOTTY:
            strncpy_k(s, "inappropriate i/o control operation", n);
            return 0;
        case ENXIO:
            strncpy_k(s, "no such device or address", n);
            return 0;
        case EOVERFLOW:
            strncpy_k(s, "value too large to be stored in data type", n);
            return 0;
        case EOWNERDEAD:
            strncpy_k(s, "previous owner died", n);
            return 0;
        case EPERM:
            strncpy_k(s, "operation not permitted", n);
            return 0;
        case EPIPE:
            strncpy_k(s, "broken pipe", n);
            return 0;
        case EPROTO:
            strncpy_k(s, "protocol error", n);
            return 0;
        case EPROTONOSUPPORT:
            strncpy_k(s, "protocol not supported", n);
            return 0;
        case EPROTOTYPE:
            strncpy_k(s, "protocol wrong type for socket", n);
            return 0;
        case ERANGE:
            strncpy_k(s, "result too large", n);
            return 0;
        case EROFS:
            strncpy_k(s, "read-only file system", n);
            return 0;
        case ESPIPE:
            strncpy_k(s, "invalid seek", n);
            return 0;
        case ESRCH:
            strncpy_k(s, "no such process", n);
            return 0;
        case ESTALE:
            strncpy_k(s, "reserved", n);
            return 0;
        case ETIME:
            strncpy_k(s, "stream ioctl() timeout", n);
            return 0;
        case ETIMEDOUT:
            strncpy_k(s, "connection timed out", n);
            return 0;
        case ETXTBSY:
            strncpy_k(s, "text file busy", n);
            return 0;
        case EXDEV:
            strncpy_k(s, "cross-device link", n);
            return 0;
        case 0:
            strncpy_k(s, "no error", n);
            return 0;
        default:
            snprintf(s, n, "unknown errnum (%d)", errnum);
            return -EINVAL;
    }
}
