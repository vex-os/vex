/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_SYS_UART_H__
#define __INCLUDE_SYS_UART_H__

#define UART_THR 0 /* Transmitter Holding Register */
#define UART_RBR 0 /* Receiver Buffer */
#define UART_DLL 0 /* Divisor Latch Low Byte */
#define UART_IER 1 /* Interrupt Enable Register */
#define UART_DLH 1 /* Divisor Latch High Byte */
#define UART_IIR 2 /* Interrupt Identification Register */
#define UART_FCR 2 /* FIFO Control Register */
#define UART_LCR 3 /* Line Control Register */
#define UART_MCR 4 /* Modem Control Register */
#define UART_LSR 5 /* Line Status Register */
#define UART_MSR 6 /* Modem Status Register */
#define UART_SCR 7 /* Scratch Register */

#define UART_IER_DAI (1 << 0) /* Received Data Available Interrupt */
#define UART_IER_TEI (1 << 1) /* Transmitter Holding Register Empty Interrupt */
#define UART_IER_RSI (1 << 2) /* Receiver Line Status Interrupt */
#define UART_IER_MSI (1 << 3) /* Modem Status Interrupt */
#define UART_IER_SLP (1 << 4) /* Sleep Mode (16750) */
#define UART_IER_LPM (1 << 5) /* Low Power Mode (16750) */
#define UART_IER_RS0 (1 << 6) /* Reserved */
#define UART_IER_RS1 (1 << 7) /* Reserved */

#define UART_IIR_IPF (1 << 0) /* Interrupt Pending Flag */
#define UART_IIR_F64 (1 << 5) /* 64-byte FIFO enabled (16750) */
#define UART_IIR_FFN (1 << 6) /* FIFO is functioning */
#define UART_IIR_FPR (1 << 7) /* FIFO is present and enabled */
#define UART_IIR_SRC_MSI 0x00 /* Modem Status Interrupt */
#define UART_IIR_SRC_TEI 0x02 /* Transmitter Holding Register Empty Interrupt */
#define UART_IIR_SRC_DAI 0x04 /* Received Data Available Interrupt */
#define UART_IIR_SRC_RSI 0x06 /* Receiver Line Status Interrupt */
#define UART_IIR_SRC_TIP 0x0C /* Time-out Interrupt Pending (16550) */
#define UART_IIR_SRC_MASK 0xE0

#define UART_FCR_FEN (1 << 0) /* Enable FIFO */
#define UART_FCR_CRF (1 << 1) /* Clear Receive FIFO */
#define UART_FCR_CTF (1 << 2) /* Clear Transmit FIFO */
#define UART_FCR_DMS (1 << 3) /* DMA Mode Select */
#define UART_FCR_F64 (1 << 5) /* Enable 64 Byte FIFO (16750) */
#define UART_FCR_TRG_01_01 0x00 /* ITL 1B, TL 1B */
#define UART_FCR_TRG_04_16 0x40 /* ITL 4B, TL 16B */
#define UART_FCR_TRG_08_32 0x80 /* ITL 8B, TL 32B */
#define UART_FCR_TRG_14_56 0xC0 /* ITL 14B, TL 56B */
#define UART_FCR_TRG_MASK 0xC0

#define UART_LCR_1SB (0 << 2) /* 1 stop bit */
#define UART_LCR_2SB (1 << 2) /* 2 (1.5) stop bits */
#define UART_LCR_SBE (1 << 6) /* Set Break Enable */
#define UART_LCR_DLB (1 << 7) /* Divisor Latch Access Bit */
#define UART_LCR_PAR_NIL 0x00 /* No parity */
#define UART_LCR_PAR_ODD 0x08 /* Odd parity */
#define UART_LCR_PAR_EVN 0x18 /* Even parity */
#define UART_LCR_PAR_MRK 0x28 /* Mark parity */
#define UART_LCR_PAR_SPC 0x38 /* Space parity */
#define UART_LCR_LEN_5 0x00 /* 5-bit word */
#define UART_LCR_LEN_6 0x01 /* 6-bit word */
#define UART_LCR_LEN_7 0x02 /* 7-bit word */
#define UART_LCR_LEN_8 0x03 /* 8-bit word */
#define UART_LCR_PAR_MASK 0x38
#define UART_LCR_LEN_MASK 0x03

#define UART_MCR_DTR (1 << 0) /* Data Terminal Ready */
#define UART_MCR_RTS (1 << 1) /* Request To Send */
#define UART_MCR_AO1 (1 << 2) /* Auxiliary Output 1 */
#define UART_MCR_AO2 (1 << 3) /* Auxiliary Output 2 */
#define UART_MCR_LBK (1 << 4) /* Loopback Mode */
#define UART_MCR_ACE (1 << 5) /* Autoflow Control Enabled (16750) */
#define UART_MCR_RS0 (1 << 6) /* Reserved */
#define UART_MCR_RS1 (1 << 7) /* Reserved */

#define UART_LSR_DATA (1 << 0) /* Data Ready Flag */
#define UART_LSR_ERUN (1 << 1) /* Overrun Error Flag */
#define UART_LSR_EPAR (1 << 3) /* Parity Error Flag */
#define UART_LSR_EFRM (1 << 4) /* Framing Error Flag */
#define UART_LSR_BRKI (1 << 5) /* Break Interrupt Flag */
#define UART_LSR_ETHR (1 << 6) /* Empty Transmitter Holding Register */
#define UART_LSR_EDHR (1 << 7) /* Empty Data Holding Registers */
#define UART_LSR_FIFO (1 << 8) /* Error in Received FIFO */

#define UART_MSR_DTS (1 << 0) /* Delta Clear To Send */
#define UART_MSR_DDS (1 << 1) /* Delta Data Set Ready */
#define UART_MSR_TER (1 << 2) /* Trailing Edge Ring Indicator */
#define UART_MSR_DDC (1 << 3) /* Delta Data Carrier Detect */
#define UART_MSR_CTS (1 << 4) /* Clear To Send */
#define UART_MSR_DSR (1 << 5) /* Data Set Ready */
#define UART_MSR_RNI (1 << 6) /* Ring Indicator */
#define UART_MSR_CRD (1 << 7) /* Carrier Detect */

#endif /* __INCLUDE_SYS_UART_H__ */
