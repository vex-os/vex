#pragma once

#include <usr/vex/serial.h>

#define SERIAL_PARITY_NONE  __k_SERIAL_PARITY_NONE
#define SERIAL_PARITY_ODD   __k_SERIAL_PARITY_ODD
#define SERIAL_PARITY_EVEN  __k_SERIAL_PARITY_EVEN
#define SERIAL_PARITY_MARK  __k_SERIAL_PARITY_MARK
#define SERIAL_PARITY_SPACE __k_SERIAL_PARITY_SPACE

#define SERIAL_FLOW_RTSCTS __k_SERIAL_FLOW_RTSCTS

#define SERIAL_MODEM_DTR __k_SERIAL_MODEM_DTR
#define SERIAL_MODEM_RTS __k_SERIAL_MODEM_RTS
#define SERIAL_MODEM_DCD __k_SERIAL_MODEM_DCD
#define SERIAL_MODEM_DSR __k_SERIAL_MODEM_DSR
#define SERIAL_MODEM_CTS __k_SERIAL_MODEM_CTS
#define SERIAL_MODEM_RI  __k_SERIAL_MODEM_RI

#define SERIAL_EXCHANGE_MAX  __k_SERIAL_EXCHANGE_MAX
#define SERIAL_NO_TERMINATOR __k_SERIAL_NO_TERMINATOR

#define SERIAL_ANY_INDEX __k_SERIAL_ANY_INDEX

#define SERIAL_GET_INFO    __k_SERIAL_GET_INFO
#define SERIAL_GET_LINE    __k_SERIAL_GET_LINE
#define SERIAL_SET_LINE    __k_SERIAL_SET_LINE
#define SERIAL_GET_MODEM   __k_SERIAL_GET_MODEM
#define SERIAL_SET_MODEM   __k_SERIAL_SET_MODEM
#define SERIAL_BREAK       __k_SERIAL_BREAK
#define SERIAL_EXCHANGE    __k_SERIAL_EXCHANGE
#define SERIAL_BIND        __k_SERIAL_BIND
#define SERIAL_UNBIND      __k_SERIAL_UNBIND
#define SERIAL_GET_BINDING __k_SERIAL_GET_BINDING

typedef struct __k_serial_info serial_info_t;
typedef struct __k_serial_line serial_line_t;
typedef struct __k_serial_modem serial_modem_t;
typedef struct __k_serial_exchange serial_exchange_t;
typedef struct __k_serial_binding serial_binding_t;
