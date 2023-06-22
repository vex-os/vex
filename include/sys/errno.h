/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_ERRNO_H__
#define __INCLUDE_SYS_ERRNO_H__

/* ISO/IEC 9899:1999 definitions */
#define EDOM            1
#define EILSEQ          2
#define ERANGE          3

/* IEEE Std 1003.1-2017 definitions */
#define E2BIG           101
#define EACCES          102
#define EADDRINUSE      103
#define EADDRNOTAVAIL   104
#define EAFNOSUPPORT    105
#define EAGAIN          106
#define EALREADY        107
#define EBADF           108
#define EBADMSG         109
#define EBUSY           110
#define ECANCELED       111
#define ECHILD          112
#define ECONNABORTED    113
#define ECONNREFUSED    114
#define ECONNRESET      115
#define EDEADLK         116
#define EDESTADDRREQ    117
#define EDQUOT          118
#define EEXIST          119
#define EFAULT          120
#define EFBIG           121
#define EHOSTUNREACH    122
#define EIDRM           123
#define EINPROGRESS     124
#define EINTR           125
#define EINVAL          126
#define EIO             127
#define EISCONN         128
#define EISDIR          129
#define ELOOP           130
#define EMFILE          131
#define EMLINK          132
#define EMSGSIZE        133
#define EMULTIHOP       134
#define ENAMETOOLONG    135
#define ENETDOWN        136
#define ENETRESET       137
#define ENETUNREACH     138
#define ENFILE          139
#define ENOBUFS         140
#define ENODATA         141
#define ENODEV          142
#define ENOENT          143
#define ENOEXEC         144
#define ENOLCK          145
#define ENOLINK         146
#define ENOMEM          147
#define ENOMSG          148
#define ENOPROTOOPT     149
#define ENOSPC          150
#define ENOSR           151
#define ENOSTR          152
#define ENOSYS          153
#define ENOTCONN        154
#define ENOTDIR         155
#define ENOTEMPTY       156
#define ENOTSOCK        157
#define ENOTSUP         158
#define ENOTTY          159
#define ENXIO           160
#define EOPNOTSUPP      161
#define EOVERFLOW       162
#define EPERM           163
#define EPIPE           164
#define EPROTO          165
#define EPROTONOSUPPORT 166
#define EPROTOTYPE      167
#define EROFS           168
#define ESPIPE          169
#define ESRCH           170
#define ESTALE          171
#define ETIME           172
#define ETIMEDOUT       173
#define ETXTBSY         174
#define EWOULDBLOCK     175
#define EXDEV           176

#endif /* __INCLUDE_SYS_ERRNO_H__ */
