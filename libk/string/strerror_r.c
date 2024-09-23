/* SPDX-License-Identifier: Zlib */
#include <sprintf.h>
#include <string.h>
#include <strings.h>
#include <vex/errno.h>

int strerror_r(int errnum, char *restrict str, size_t sz)
{
    switch(errnum) {
        case EDOM:
            kstrncpy(str, "argument out of domain of function", sz);
            return 0;
        case EILSEQ:
            kstrncpy(str, "illegal byte sequence", sz);
            return 0;
        case ERANGE:
            kstrncpy(str, "result too large", sz);
            return 0;
        case E2BIG:
            kstrncpy(str, "argument list too long", sz);
            return 0;
        case EACCES:
            kstrncpy(str, "permission denied", sz);
            return 0;
        case EADDRINUSE:
            kstrncpy(str, "address in use", sz);
            return 0;
        case EADDRNOTAVAIL:
            kstrncpy(str, "address not available", sz);
            return 0;
        case EAFNOSUPPORT:
            kstrncpy(str, "address family not supported", sz);
            return 0;
        case EAGAIN:
            kstrncpy(str, "resource temporarily unavailable", sz);
            return 0;
        case EALREADY:
            kstrncpy(str, "connection already in progress", sz);
            return 0;
        case EBADF:
            kstrncpy(str, "bad file descriptor", sz);
            return 0;
        case EBADMSG:
            kstrncpy(str, "bad message", sz);
            return 0;
        case EBUSY:
            kstrncpy(str, "device or resource busy", sz);
            return 0;
        case ECANCELED:
            kstrncpy(str, "operation canceled", sz);
            return 0;
        case ECHILD:
            kstrncpy(str, "no child processes", sz);
            return 0;
        case ECONNABORTED:
            kstrncpy(str, "connection aborted", sz);
            return 0;
        case ECONNREFUSED:
            kstrncpy(str, "connection refused", sz);
            return 0;
        case ECONNRESET:
            kstrncpy(str, "connection reset", sz);
            return 0;
        case EDEADLK:
            kstrncpy(str, "resource deadlock would occur", sz);
            return 0;
        case EDESTADDRREQ:
            kstrncpy(str, "destination address required", sz);
            return 0;
        case EDQUOT:
            kstrncpy(str, "disk quota exceeded", sz);
            return 0;
        case EEXIST:
            kstrncpy(str, "file exists", sz);
            return 0;
        case EFAULT:
            kstrncpy(str, "bad address", sz);
            return 0;
        case EFBIG:
            kstrncpy(str, "file too large", sz);
            return 0;
        case EHOSTUNREACH:
            kstrncpy(str, "host is unreachable", sz);
            return 0;
        case EIDRM:
            kstrncpy(str, "identifier removed", sz);
            return 0;
        case EINPROGRESS:
            kstrncpy(str, "operation in progress", sz);
            return 0;
        case EINTR:
            kstrncpy(str, "interrupted function call", sz);
            return 0;
        case EINVAL:
            kstrncpy(str, "invalid argument", sz);
            return 0;
        case EIO:
            kstrncpy(str, "input/output error", sz);
            return 0;
        case EISCONN:
            kstrncpy(str, "socket is connected", sz);
            return 0;
        case EISDIR:
            kstrncpy(str, "is a directory", sz);
            return 0;
        case ELOOP:
            kstrncpy(str, "too many levels of symbolic links", sz);
            return 0;
        case EMFILE:
            kstrncpy(str, "too many open files", sz);
            return 0;
        case EMLINK:
            kstrncpy(str, "too many links", sz);
            return 0;
        case EMSGSIZE:
            kstrncpy(str, "message too long", sz);
            return 0;
        case EMULTIHOP:
            kstrncpy(str, "multihop attempted", sz);
            return 0;
        case ENAMETOOLONG:
            kstrncpy(str, "filename too long", sz);
            return 0;
        case ENETDOWN:
            kstrncpy(str, "network is down", sz);
            return 0;
        case ENETRESET:
            kstrncpy(str, "connection aborted by network", sz);
            return 0;
        case ENETUNREACH:
            kstrncpy(str, "network unreachable", sz);
            return 0;
        case ENFILE:
            kstrncpy(str, "too many files open in system", sz);
            return 0;
        case ENOBUFS:
            kstrncpy(str, "no buffer space available", sz);
            return 0;
        case ENODATA:
            kstrncpy(str, "no message is available on the stream head read queue", sz);
            return 0;
        case ENODEV:
            kstrncpy(str, "no such device", sz);
            return 0;
        case ENOENT:
            kstrncpy(str, "no such file or directory", sz);
            return 0;
        case ENOEXEC:
            kstrncpy(str, "exec format error", sz);
            return 0;
        case ENOLCK:
            kstrncpy(str, "no locks available", sz);
            return 0;
        case ENOLINK:
            kstrncpy(str, "link has been severed", sz);
            return 0;
        case ENOMEM:
            kstrncpy(str, "not enough space", sz);
            return 0;
        case ENOMSG:
            kstrncpy(str, "no message of the desired type", sz);
            return 0;
        case ENOPROTOOPT:
            kstrncpy(str, "protocol not available", sz);
            return 0;
        case ENOSPC:
            kstrncpy(str, "no space left on device", sz);
            return 0;
        case ENOSR:
            kstrncpy(str, "no stream resources", sz);
            return 0;
        case ENOSTR:
            kstrncpy(str, "not a stream", sz);
            return 0;
        case ENOSYS:
            kstrncpy(str, "function not implemented", sz);
            return 0;
        case ENOTCONN:
            kstrncpy(str, "the socket is not connected", sz);
            return 0;
        case ENOTDIR:
            kstrncpy(str, "not a directory", sz);
            return 0;
        case ENOTEMPTY:
            kstrncpy(str, "directory not empty", sz);
            return 0;
        case ENOTSOCK:
            kstrncpy(str, "not a socket", sz);
            return 0;
        case ENOTSUP:
            kstrncpy(str, "operation not supported", sz);
            return 0;
        case ENOTTY:
            kstrncpy(str, "inappropriate i/o control operation", sz);
            return 0;
        case ENXIO:
            kstrncpy(str, "no such device or address", sz);
            return 0;
        case EOPNOTSUPP:
            kstrncpy(str, "operation not supported on socket", sz);
            return 0;
        case EOVERFLOW:
            kstrncpy(str, "value too large to be stored in data type", sz);
            return 0;
        case EPERM:
            kstrncpy(str, "operation not permitted", sz);
            return 0;
        case EPIPE:
            kstrncpy(str, "broken pipe", sz);
            return 0;
        case EPROTO:
            kstrncpy(str, "protocol error", sz);
            return 0;
        case EPROTONOSUPPORT:
            kstrncpy(str, "protocol not supported", sz);
            return 0;
        case EPROTOTYPE:
            kstrncpy(str, "protocol wrong type for socket", sz);
            return 0;
        case EROFS:
            kstrncpy(str, "read-only file system", sz);
            return 0;
        case ESPIPE:
            kstrncpy(str, "invalid seek", sz);
            return 0;
        case ESRCH:
            kstrncpy(str, "no such process", sz);
            return 0;
        case ESTALE:
            kstrncpy(str, "stale file handle", sz);
            return 0;
        case ETIME:
            kstrncpy(str, "stream ioctl() timeout", sz);
            return 0;
        case ETIMEDOUT:
            kstrncpy(str, "connection timed out", sz);
            return 0;
        case ETXTBSY:
            kstrncpy(str, "text file busy", sz);
            return 0;
        case EWOULDBLOCK:
            kstrncpy(str, "operation would block", sz);
            return 0;
        case EXDEV:
            kstrncpy(str, "cross-device link", sz);
            return 0;
        case 0:
            kstrncpy(str, "no error", sz);
            return 0;
        default:
            snprintf(str, sz, "unknown errnum (%d)", errnum);
            return EINVAL;
    }
}
