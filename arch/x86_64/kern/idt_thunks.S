## SPDX-License-Identifier: BSD-2-Clause
.section .text

.macro push_interrupt_frame
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax
.endm

.macro pop_interrupt_frame
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
.endm

.macro isr_stub stubname vector
    .global \stubname
    .type \stubname, @function
    \stubname:
        push_interrupt_frame

        movq %rsp, %rdi
        movq $\vector, %rsi
        call x86_isr_handler

        pop_interrupt_frame
        addq $0x08, %rsp
        iretq
.endm

.macro isr_stub_pz stubname vector
    .global \stubname
    .type \stubname, @function
    \stubname:
        pushq $0x0000000000000000
        push_interrupt_frame

        movq %rsp, %rdi
        movq $\vector, %rsi
        call x86_isr_handler

        pop_interrupt_frame
        addq $0x08, %rsp
        iretq
.endm

.macro intreq_stub stubname vector
    .global \stubname
    .type \stubname, @function
    \stubname:
        pushq $0x0000000000000000
        push_interrupt_frame

        movq %rsp, %rdi
        movq $\vector, %rsi
        call x86_intreq_handler

        pop_interrupt_frame
        addq $0x08, %rsp
        iretq
.endm

isr_stub    x86_isr_00, 0x00
isr_stub    x86_isr_01, 0x01
isr_stub    x86_isr_02, 0x02
isr_stub    x86_isr_03, 0x03
isr_stub    x86_isr_04, 0x04
isr_stub    x86_isr_05, 0x05
isr_stub    x86_isr_06, 0x06
isr_stub    x86_isr_07, 0x07
isr_stub_pz x86_isr_08, 0x08
isr_stub    x86_isr_09, 0x09
isr_stub_pz x86_isr_0A, 0x0A
isr_stub_pz x86_isr_0B, 0x0B
isr_stub_pz x86_isr_0C, 0x0C
isr_stub_pz x86_isr_0D, 0x0D
isr_stub_pz x86_isr_0E, 0x0E
isr_stub    x86_isr_0F, 0x0F
isr_stub    x86_isr_10, 0x10
isr_stub    x86_isr_11, 0x11
isr_stub    x86_isr_12, 0x12
isr_stub    x86_isr_13, 0x13
isr_stub    x86_isr_14, 0x14
isr_stub_pz x86_isr_15, 0x15
isr_stub    x86_isr_16, 0x16
isr_stub    x86_isr_17, 0x17
isr_stub    x86_isr_18, 0x18
isr_stub    x86_isr_19, 0x19
isr_stub    x86_isr_1A, 0x1A
isr_stub    x86_isr_1B, 0x1B
isr_stub    x86_isr_1C, 0x1C
isr_stub    x86_isr_1D, 0x1D
isr_stub    x86_isr_1E, 0x1E
isr_stub    x86_isr_1F, 0x1F

intreq_stub x86_intreq_20, 0x20
intreq_stub x86_intreq_21, 0x21
intreq_stub x86_intreq_22, 0x22
intreq_stub x86_intreq_23, 0x23
intreq_stub x86_intreq_24, 0x24
intreq_stub x86_intreq_25, 0x25
intreq_stub x86_intreq_26, 0x26
intreq_stub x86_intreq_27, 0x27
intreq_stub x86_intreq_28, 0x28
intreq_stub x86_intreq_29, 0x29
intreq_stub x86_intreq_2A, 0x2A
intreq_stub x86_intreq_2B, 0x2B
intreq_stub x86_intreq_2C, 0x2C
intreq_stub x86_intreq_2D, 0x2D
intreq_stub x86_intreq_2E, 0x2E
intreq_stub x86_intreq_2F, 0x2F
intreq_stub x86_intreq_30, 0x30
intreq_stub x86_intreq_31, 0x31
intreq_stub x86_intreq_32, 0x32
intreq_stub x86_intreq_33, 0x33
intreq_stub x86_intreq_34, 0x34
intreq_stub x86_intreq_35, 0x35
intreq_stub x86_intreq_36, 0x36
intreq_stub x86_intreq_37, 0x37
intreq_stub x86_intreq_38, 0x38
intreq_stub x86_intreq_39, 0x39
intreq_stub x86_intreq_3A, 0x3A
intreq_stub x86_intreq_3B, 0x3B
intreq_stub x86_intreq_3C, 0x3C
intreq_stub x86_intreq_3D, 0x3D
intreq_stub x86_intreq_3E, 0x3E
intreq_stub x86_intreq_3F, 0x3F
intreq_stub x86_intreq_40, 0x40
intreq_stub x86_intreq_41, 0x41
intreq_stub x86_intreq_42, 0x42
intreq_stub x86_intreq_43, 0x43
intreq_stub x86_intreq_44, 0x44
intreq_stub x86_intreq_45, 0x45
intreq_stub x86_intreq_46, 0x46
intreq_stub x86_intreq_47, 0x47
intreq_stub x86_intreq_48, 0x48
intreq_stub x86_intreq_49, 0x49
intreq_stub x86_intreq_4A, 0x4A
intreq_stub x86_intreq_4B, 0x4B
intreq_stub x86_intreq_4C, 0x4C
intreq_stub x86_intreq_4D, 0x4D
intreq_stub x86_intreq_4E, 0x4E
intreq_stub x86_intreq_4F, 0x4F
intreq_stub x86_intreq_50, 0x50
intreq_stub x86_intreq_51, 0x51
intreq_stub x86_intreq_52, 0x52
intreq_stub x86_intreq_53, 0x53
intreq_stub x86_intreq_54, 0x54
intreq_stub x86_intreq_55, 0x55
intreq_stub x86_intreq_56, 0x56
intreq_stub x86_intreq_57, 0x57
intreq_stub x86_intreq_58, 0x58
intreq_stub x86_intreq_59, 0x59
intreq_stub x86_intreq_5A, 0x5A
intreq_stub x86_intreq_5B, 0x5B
intreq_stub x86_intreq_5C, 0x5C
intreq_stub x86_intreq_5D, 0x5D
intreq_stub x86_intreq_5E, 0x5E
intreq_stub x86_intreq_5F, 0x5F
intreq_stub x86_intreq_60, 0x60
intreq_stub x86_intreq_61, 0x61
intreq_stub x86_intreq_62, 0x62
intreq_stub x86_intreq_63, 0x63
intreq_stub x86_intreq_64, 0x64
intreq_stub x86_intreq_65, 0x65
intreq_stub x86_intreq_66, 0x66
intreq_stub x86_intreq_67, 0x67
intreq_stub x86_intreq_68, 0x68
intreq_stub x86_intreq_69, 0x69
intreq_stub x86_intreq_6A, 0x6A
intreq_stub x86_intreq_6B, 0x6B
intreq_stub x86_intreq_6C, 0x6C
intreq_stub x86_intreq_6D, 0x6D
intreq_stub x86_intreq_6E, 0x6E
intreq_stub x86_intreq_6F, 0x6F
intreq_stub x86_intreq_70, 0x70
intreq_stub x86_intreq_71, 0x71
intreq_stub x86_intreq_72, 0x72
intreq_stub x86_intreq_73, 0x73
intreq_stub x86_intreq_74, 0x74
intreq_stub x86_intreq_75, 0x75
intreq_stub x86_intreq_76, 0x76
intreq_stub x86_intreq_77, 0x77
intreq_stub x86_intreq_78, 0x78
intreq_stub x86_intreq_79, 0x79
intreq_stub x86_intreq_7A, 0x7A
intreq_stub x86_intreq_7B, 0x7B
intreq_stub x86_intreq_7C, 0x7C
intreq_stub x86_intreq_7D, 0x7D
intreq_stub x86_intreq_7E, 0x7E
intreq_stub x86_intreq_7F, 0x7F
intreq_stub x86_intreq_80, 0x80
intreq_stub x86_intreq_81, 0x81
intreq_stub x86_intreq_82, 0x82
intreq_stub x86_intreq_83, 0x83
intreq_stub x86_intreq_84, 0x84
intreq_stub x86_intreq_85, 0x85
intreq_stub x86_intreq_86, 0x86
intreq_stub x86_intreq_87, 0x87
intreq_stub x86_intreq_88, 0x88
intreq_stub x86_intreq_89, 0x89
intreq_stub x86_intreq_8A, 0x8A
intreq_stub x86_intreq_8B, 0x8B
intreq_stub x86_intreq_8C, 0x8C
intreq_stub x86_intreq_8D, 0x8D
intreq_stub x86_intreq_8E, 0x8E
intreq_stub x86_intreq_8F, 0x8F
intreq_stub x86_intreq_90, 0x90
intreq_stub x86_intreq_91, 0x91
intreq_stub x86_intreq_92, 0x92
intreq_stub x86_intreq_93, 0x93
intreq_stub x86_intreq_94, 0x94
intreq_stub x86_intreq_95, 0x95
intreq_stub x86_intreq_96, 0x96
intreq_stub x86_intreq_97, 0x97
intreq_stub x86_intreq_98, 0x98
intreq_stub x86_intreq_99, 0x99
intreq_stub x86_intreq_9A, 0x9A
intreq_stub x86_intreq_9B, 0x9B
intreq_stub x86_intreq_9C, 0x9C
intreq_stub x86_intreq_9D, 0x9D
intreq_stub x86_intreq_9E, 0x9E
intreq_stub x86_intreq_9F, 0x9F
intreq_stub x86_intreq_A0, 0xA0
intreq_stub x86_intreq_A1, 0xA1
intreq_stub x86_intreq_A2, 0xA2
intreq_stub x86_intreq_A3, 0xA3
intreq_stub x86_intreq_A4, 0xA4
intreq_stub x86_intreq_A5, 0xA5
intreq_stub x86_intreq_A6, 0xA6
intreq_stub x86_intreq_A7, 0xA7
intreq_stub x86_intreq_A8, 0xA8
intreq_stub x86_intreq_A9, 0xA9
intreq_stub x86_intreq_AA, 0xAA
intreq_stub x86_intreq_AB, 0xAB
intreq_stub x86_intreq_AC, 0xAC
intreq_stub x86_intreq_AD, 0xAD
intreq_stub x86_intreq_AE, 0xAE
intreq_stub x86_intreq_AF, 0xAF
intreq_stub x86_intreq_B0, 0xB0
intreq_stub x86_intreq_B1, 0xB1
intreq_stub x86_intreq_B2, 0xB2
intreq_stub x86_intreq_B3, 0xB3
intreq_stub x86_intreq_B4, 0xB4
intreq_stub x86_intreq_B5, 0xB5
intreq_stub x86_intreq_B6, 0xB6
intreq_stub x86_intreq_B7, 0xB7
intreq_stub x86_intreq_B8, 0xB8
intreq_stub x86_intreq_B9, 0xB9
intreq_stub x86_intreq_BA, 0xBA
intreq_stub x86_intreq_BB, 0xBB
intreq_stub x86_intreq_BC, 0xBC
intreq_stub x86_intreq_BD, 0xBD
intreq_stub x86_intreq_BE, 0xBE
intreq_stub x86_intreq_BF, 0xBF
intreq_stub x86_intreq_C0, 0xC0
intreq_stub x86_intreq_C1, 0xC1
intreq_stub x86_intreq_C2, 0xC2
intreq_stub x86_intreq_C3, 0xC3
intreq_stub x86_intreq_C4, 0xC4
intreq_stub x86_intreq_C5, 0xC5
intreq_stub x86_intreq_C6, 0xC6
intreq_stub x86_intreq_C7, 0xC7
intreq_stub x86_intreq_C8, 0xC8
intreq_stub x86_intreq_C9, 0xC9
intreq_stub x86_intreq_CA, 0xCA
intreq_stub x86_intreq_CB, 0xCB
intreq_stub x86_intreq_CC, 0xCC
intreq_stub x86_intreq_CD, 0xCD
intreq_stub x86_intreq_CE, 0xCE
intreq_stub x86_intreq_CF, 0xCF
intreq_stub x86_intreq_D0, 0xD0
intreq_stub x86_intreq_D1, 0xD1
intreq_stub x86_intreq_D2, 0xD2
intreq_stub x86_intreq_D3, 0xD3
intreq_stub x86_intreq_D4, 0xD4
intreq_stub x86_intreq_D5, 0xD5
intreq_stub x86_intreq_D6, 0xD6
intreq_stub x86_intreq_D7, 0xD7
intreq_stub x86_intreq_D8, 0xD8
intreq_stub x86_intreq_D9, 0xD9
intreq_stub x86_intreq_DA, 0xDA
intreq_stub x86_intreq_DB, 0xDB
intreq_stub x86_intreq_DC, 0xDC
intreq_stub x86_intreq_DD, 0xDD
intreq_stub x86_intreq_DE, 0xDE
intreq_stub x86_intreq_DF, 0xDF
intreq_stub x86_intreq_E0, 0xE0
intreq_stub x86_intreq_E1, 0xE1
intreq_stub x86_intreq_E2, 0xE2
intreq_stub x86_intreq_E3, 0xE3
intreq_stub x86_intreq_E4, 0xE4
intreq_stub x86_intreq_E5, 0xE5
intreq_stub x86_intreq_E6, 0xE6
intreq_stub x86_intreq_E7, 0xE7
intreq_stub x86_intreq_E8, 0xE8
intreq_stub x86_intreq_E9, 0xE9
intreq_stub x86_intreq_EA, 0xEA
intreq_stub x86_intreq_EB, 0xEB
intreq_stub x86_intreq_EC, 0xEC
intreq_stub x86_intreq_ED, 0xED
intreq_stub x86_intreq_EE, 0xEE
intreq_stub x86_intreq_EF, 0xEF
intreq_stub x86_intreq_F0, 0xF0
intreq_stub x86_intreq_F1, 0xF1
intreq_stub x86_intreq_F2, 0xF2
intreq_stub x86_intreq_F3, 0xF3
intreq_stub x86_intreq_F4, 0xF4
intreq_stub x86_intreq_F5, 0xF5
intreq_stub x86_intreq_F6, 0xF6
intreq_stub x86_intreq_F7, 0xF7
intreq_stub x86_intreq_F8, 0xF8
intreq_stub x86_intreq_F9, 0xF9
intreq_stub x86_intreq_FA, 0xFA
intreq_stub x86_intreq_FB, 0xFB
intreq_stub x86_intreq_FC, 0xFC
intreq_stub x86_intreq_FD, 0xFD
intreq_stub x86_intreq_FE, 0xFE
intreq_stub x86_intreq_FF, 0xFF
