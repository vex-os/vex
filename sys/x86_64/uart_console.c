/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <errno.h>
#include <string.h>
#include <psys/console.h>
#include <psys/initcall.h>
#include <psys/klog.h>
#include <psys/x86_64/pmio.h>
#include <psys/uart.h>

#define PC_UART_TEST 0xAE
#define PC_UART_CLOCK 115200
#define CONSOLE_SPEED 9600

#define from_driver_data(ptr) ((pmio_addr_t)((uintptr_t)(ptr)))
#define to_driver_data(value) ((void *)((uintptr_t)(value)))

static int uart_console_init(struct console *con)
{
    pmio_addr_t base = from_driver_data(con->driver_data);
    pmio_uint16_t d;

    pmio_write8(base + UART_SCR, PC_UART_TEST);
    if(pmio_read8(base + UART_SCR) != PC_UART_TEST) {
        /* not present */
        return -ENODEV;
    }

    pmio_write8(base + UART_IER, 0);

    d = PC_UART_CLOCK / CONSOLE_SPEED;
    pmio_write8(base + UART_LCR, UART_LCR_DLB);
    pmio_write8(base + UART_DLL, (d & 0x00FF));
    pmio_write8(base + UART_DLH, (d & 0xFF00) >> 8);

    /* 8-bit words, 1 stop bit, no parity, FIFO with girthiest threshold */
    pmio_write8(base + UART_LCR, UART_LCR_LEN_8 | UART_LCR_1SB | UART_LCR_PAR_NIL);
    pmio_write8(base + UART_FCR, UART_FCR_FEN | UART_FCR_CRF | UART_FCR_CTF | UART_FCR_TRG_14_56);

    pmio_write8(base + UART_MCR, UART_MCR_RTS | UART_MCR_AO1 | UART_MCR_AO2 | UART_MCR_LBK);
    pmio_write8_throttle(base + UART_THR, PC_UART_TEST);
    if(pmio_read8_throttle(base + UART_RBR) != PC_UART_TEST) {
        /* faulty */
        return -ENODEV;
    }

    pmio_write8(base + UART_MCR, UART_MCR_DTR | UART_MCR_RTS | UART_MCR_AO1 | UART_MCR_AO2);

    return 0;
}

static size_t uart_console_write(struct console *con, const void *s, size_t n)
{
    size_t i;
    const pmio_uint8_t *sp = s;
    pmio_addr_t base = from_driver_data(con->driver_data);
    for(i = 0; i < n; i++) {
        while((pmio_read8(base + UART_LSR) & UART_LSR_ETHR) != UART_LSR_ETHR);
        pmio_write8(base + UART_THR, sp[i]);
    }

    return n;
}

#define NUM_UART_CONSOLES 4
static struct console uart_consoles[NUM_UART_CONSOLES] = {
    {
        .name = "tty1",
        .driver_data = to_driver_data(0x3F8),
        .init_fn = &uart_console_init,
        .write_fn = &uart_console_write,
        .next = NULL
    },
    {
        .name = "tty2",
        .driver_data = to_driver_data(0x2F8),
        .init_fn = &uart_console_init,
        .write_fn = &uart_console_write,
        .next = NULL
    },
    {
        .name = "tty3",
        .driver_data = to_driver_data(0x3E8),
        .init_fn = &uart_console_init,
        .write_fn = &uart_console_write,
        .next = NULL
    },
    {
        .name = "tty4",
        .driver_data = to_driver_data(0x2E8),
        .init_fn = &uart_console_init,
        .write_fn = &uart_console_write,
        .next = NULL
    },
};

static void init_uart_console(void)
{
    unsigned int i;
    for(i = 0; i < NUM_UART_CONSOLES; i++)
        register_console(&uart_consoles[i]);
}
initcall_tier_0(uart_console, init_uart_console);
