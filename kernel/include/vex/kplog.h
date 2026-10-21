#pragma once

#include <usr/vex/kplog.h>

#define KPLOG_EMERG    __k_KPLOG_EMERG
#define KPLOG_ALERT    __k_KPLOG_ALERT
#define KPLOG_CRITICAL __k_KPLOG_CRITICAL
#define KPLOG_ERROR    __k_KPLOG_ERROR
#define KPLOG_WARNING  __k_KPLOG_WARNING
#define KPLOG_NOTICE   __k_KPLOG_NOTICE
#define KPLOG_INFORM   __k_KPLOG_INFORM
#define KPLOG_VERBOSE  __k_KPLOG_VERBOSE

#define KPLOG_TRUNCATED __k_KPLOG_TRUNCATED

typedef struct __k_kplog_record kplog_record_t;
