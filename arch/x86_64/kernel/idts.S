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

.macro ireq_stub stubname vector
    .global \stubname
    .type \stubname, @function
    \stubname:
        pushq $0x0000000000000000
        push_interrupt_frame

        movq %rsp, %rdi
        movq $\vector, %rsi
        call x86_ireq_handler

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

ireq_stub x86_ireq_20, 0x20
ireq_stub x86_ireq_21, 0x21
ireq_stub x86_ireq_22, 0x22
ireq_stub x86_ireq_23, 0x23
ireq_stub x86_ireq_24, 0x24
ireq_stub x86_ireq_25, 0x25
ireq_stub x86_ireq_26, 0x26
ireq_stub x86_ireq_27, 0x27
ireq_stub x86_ireq_28, 0x28
ireq_stub x86_ireq_29, 0x29
ireq_stub x86_ireq_2A, 0x2A
ireq_stub x86_ireq_2B, 0x2B
ireq_stub x86_ireq_2C, 0x2C
ireq_stub x86_ireq_2D, 0x2D
ireq_stub x86_ireq_2E, 0x2E
ireq_stub x86_ireq_2F, 0x2F
ireq_stub x86_ireq_30, 0x30
ireq_stub x86_ireq_31, 0x31
ireq_stub x86_ireq_32, 0x32
ireq_stub x86_ireq_33, 0x33
ireq_stub x86_ireq_34, 0x34
ireq_stub x86_ireq_35, 0x35
ireq_stub x86_ireq_36, 0x36
ireq_stub x86_ireq_37, 0x37
ireq_stub x86_ireq_38, 0x38
ireq_stub x86_ireq_39, 0x39
ireq_stub x86_ireq_3A, 0x3A
ireq_stub x86_ireq_3B, 0x3B
ireq_stub x86_ireq_3C, 0x3C
ireq_stub x86_ireq_3D, 0x3D
ireq_stub x86_ireq_3E, 0x3E
ireq_stub x86_ireq_3F, 0x3F
ireq_stub x86_ireq_40, 0x40
ireq_stub x86_ireq_41, 0x41
ireq_stub x86_ireq_42, 0x42
ireq_stub x86_ireq_43, 0x43
ireq_stub x86_ireq_44, 0x44
ireq_stub x86_ireq_45, 0x45
ireq_stub x86_ireq_46, 0x46
ireq_stub x86_ireq_47, 0x47
ireq_stub x86_ireq_48, 0x48
ireq_stub x86_ireq_49, 0x49
ireq_stub x86_ireq_4A, 0x4A
ireq_stub x86_ireq_4B, 0x4B
ireq_stub x86_ireq_4C, 0x4C
ireq_stub x86_ireq_4D, 0x4D
ireq_stub x86_ireq_4E, 0x4E
ireq_stub x86_ireq_4F, 0x4F
ireq_stub x86_ireq_50, 0x50
ireq_stub x86_ireq_51, 0x51
ireq_stub x86_ireq_52, 0x52
ireq_stub x86_ireq_53, 0x53
ireq_stub x86_ireq_54, 0x54
ireq_stub x86_ireq_55, 0x55
ireq_stub x86_ireq_56, 0x56
ireq_stub x86_ireq_57, 0x57
ireq_stub x86_ireq_58, 0x58
ireq_stub x86_ireq_59, 0x59
ireq_stub x86_ireq_5A, 0x5A
ireq_stub x86_ireq_5B, 0x5B
ireq_stub x86_ireq_5C, 0x5C
ireq_stub x86_ireq_5D, 0x5D
ireq_stub x86_ireq_5E, 0x5E
ireq_stub x86_ireq_5F, 0x5F
ireq_stub x86_ireq_60, 0x60
ireq_stub x86_ireq_61, 0x61
ireq_stub x86_ireq_62, 0x62
ireq_stub x86_ireq_63, 0x63
ireq_stub x86_ireq_64, 0x64
ireq_stub x86_ireq_65, 0x65
ireq_stub x86_ireq_66, 0x66
ireq_stub x86_ireq_67, 0x67
ireq_stub x86_ireq_68, 0x68
ireq_stub x86_ireq_69, 0x69
ireq_stub x86_ireq_6A, 0x6A
ireq_stub x86_ireq_6B, 0x6B
ireq_stub x86_ireq_6C, 0x6C
ireq_stub x86_ireq_6D, 0x6D
ireq_stub x86_ireq_6E, 0x6E
ireq_stub x86_ireq_6F, 0x6F
ireq_stub x86_ireq_70, 0x70
ireq_stub x86_ireq_71, 0x71
ireq_stub x86_ireq_72, 0x72
ireq_stub x86_ireq_73, 0x73
ireq_stub x86_ireq_74, 0x74
ireq_stub x86_ireq_75, 0x75
ireq_stub x86_ireq_76, 0x76
ireq_stub x86_ireq_77, 0x77
ireq_stub x86_ireq_78, 0x78
ireq_stub x86_ireq_79, 0x79
ireq_stub x86_ireq_7A, 0x7A
ireq_stub x86_ireq_7B, 0x7B
ireq_stub x86_ireq_7C, 0x7C
ireq_stub x86_ireq_7D, 0x7D
ireq_stub x86_ireq_7E, 0x7E
ireq_stub x86_ireq_7F, 0x7F
ireq_stub x86_ireq_80, 0x80
ireq_stub x86_ireq_81, 0x81
ireq_stub x86_ireq_82, 0x82
ireq_stub x86_ireq_83, 0x83
ireq_stub x86_ireq_84, 0x84
ireq_stub x86_ireq_85, 0x85
ireq_stub x86_ireq_86, 0x86
ireq_stub x86_ireq_87, 0x87
ireq_stub x86_ireq_88, 0x88
ireq_stub x86_ireq_89, 0x89
ireq_stub x86_ireq_8A, 0x8A
ireq_stub x86_ireq_8B, 0x8B
ireq_stub x86_ireq_8C, 0x8C
ireq_stub x86_ireq_8D, 0x8D
ireq_stub x86_ireq_8E, 0x8E
ireq_stub x86_ireq_8F, 0x8F
ireq_stub x86_ireq_90, 0x90
ireq_stub x86_ireq_91, 0x91
ireq_stub x86_ireq_92, 0x92
ireq_stub x86_ireq_93, 0x93
ireq_stub x86_ireq_94, 0x94
ireq_stub x86_ireq_95, 0x95
ireq_stub x86_ireq_96, 0x96
ireq_stub x86_ireq_97, 0x97
ireq_stub x86_ireq_98, 0x98
ireq_stub x86_ireq_99, 0x99
ireq_stub x86_ireq_9A, 0x9A
ireq_stub x86_ireq_9B, 0x9B
ireq_stub x86_ireq_9C, 0x9C
ireq_stub x86_ireq_9D, 0x9D
ireq_stub x86_ireq_9E, 0x9E
ireq_stub x86_ireq_9F, 0x9F
ireq_stub x86_ireq_A0, 0xA0
ireq_stub x86_ireq_A1, 0xA1
ireq_stub x86_ireq_A2, 0xA2
ireq_stub x86_ireq_A3, 0xA3
ireq_stub x86_ireq_A4, 0xA4
ireq_stub x86_ireq_A5, 0xA5
ireq_stub x86_ireq_A6, 0xA6
ireq_stub x86_ireq_A7, 0xA7
ireq_stub x86_ireq_A8, 0xA8
ireq_stub x86_ireq_A9, 0xA9
ireq_stub x86_ireq_AA, 0xAA
ireq_stub x86_ireq_AB, 0xAB
ireq_stub x86_ireq_AC, 0xAC
ireq_stub x86_ireq_AD, 0xAD
ireq_stub x86_ireq_AE, 0xAE
ireq_stub x86_ireq_AF, 0xAF
ireq_stub x86_ireq_B0, 0xB0
ireq_stub x86_ireq_B1, 0xB1
ireq_stub x86_ireq_B2, 0xB2
ireq_stub x86_ireq_B3, 0xB3
ireq_stub x86_ireq_B4, 0xB4
ireq_stub x86_ireq_B5, 0xB5
ireq_stub x86_ireq_B6, 0xB6
ireq_stub x86_ireq_B7, 0xB7
ireq_stub x86_ireq_B8, 0xB8
ireq_stub x86_ireq_B9, 0xB9
ireq_stub x86_ireq_BA, 0xBA
ireq_stub x86_ireq_BB, 0xBB
ireq_stub x86_ireq_BC, 0xBC
ireq_stub x86_ireq_BD, 0xBD
ireq_stub x86_ireq_BE, 0xBE
ireq_stub x86_ireq_BF, 0xBF
ireq_stub x86_ireq_C0, 0xC0
ireq_stub x86_ireq_C1, 0xC1
ireq_stub x86_ireq_C2, 0xC2
ireq_stub x86_ireq_C3, 0xC3
ireq_stub x86_ireq_C4, 0xC4
ireq_stub x86_ireq_C5, 0xC5
ireq_stub x86_ireq_C6, 0xC6
ireq_stub x86_ireq_C7, 0xC7
ireq_stub x86_ireq_C8, 0xC8
ireq_stub x86_ireq_C9, 0xC9
ireq_stub x86_ireq_CA, 0xCA
ireq_stub x86_ireq_CB, 0xCB
ireq_stub x86_ireq_CC, 0xCC
ireq_stub x86_ireq_CD, 0xCD
ireq_stub x86_ireq_CE, 0xCE
ireq_stub x86_ireq_CF, 0xCF
ireq_stub x86_ireq_D0, 0xD0
ireq_stub x86_ireq_D1, 0xD1
ireq_stub x86_ireq_D2, 0xD2
ireq_stub x86_ireq_D3, 0xD3
ireq_stub x86_ireq_D4, 0xD4
ireq_stub x86_ireq_D5, 0xD5
ireq_stub x86_ireq_D6, 0xD6
ireq_stub x86_ireq_D7, 0xD7
ireq_stub x86_ireq_D8, 0xD8
ireq_stub x86_ireq_D9, 0xD9
ireq_stub x86_ireq_DA, 0xDA
ireq_stub x86_ireq_DB, 0xDB
ireq_stub x86_ireq_DC, 0xDC
ireq_stub x86_ireq_DD, 0xDD
ireq_stub x86_ireq_DE, 0xDE
ireq_stub x86_ireq_DF, 0xDF
ireq_stub x86_ireq_E0, 0xE0
ireq_stub x86_ireq_E1, 0xE1
ireq_stub x86_ireq_E2, 0xE2
ireq_stub x86_ireq_E3, 0xE3
ireq_stub x86_ireq_E4, 0xE4
ireq_stub x86_ireq_E5, 0xE5
ireq_stub x86_ireq_E6, 0xE6
ireq_stub x86_ireq_E7, 0xE7
ireq_stub x86_ireq_E8, 0xE8
ireq_stub x86_ireq_E9, 0xE9
ireq_stub x86_ireq_EA, 0xEA
ireq_stub x86_ireq_EB, 0xEB
ireq_stub x86_ireq_EC, 0xEC
ireq_stub x86_ireq_ED, 0xED
ireq_stub x86_ireq_EE, 0xEE
ireq_stub x86_ireq_EF, 0xEF
ireq_stub x86_ireq_F0, 0xF0
ireq_stub x86_ireq_F1, 0xF1
ireq_stub x86_ireq_F2, 0xF2
ireq_stub x86_ireq_F3, 0xF3
ireq_stub x86_ireq_F4, 0xF4
ireq_stub x86_ireq_F5, 0xF5
ireq_stub x86_ireq_F6, 0xF6
ireq_stub x86_ireq_F7, 0xF7
ireq_stub x86_ireq_F8, 0xF8
ireq_stub x86_ireq_F9, 0xF9
ireq_stub x86_ireq_FA, 0xFA
ireq_stub x86_ireq_FB, 0xFB
ireq_stub x86_ireq_FC, 0xFC
ireq_stub x86_ireq_FD, 0xFD
ireq_stub x86_ireq_FE, 0xFE
ireq_stub x86_ireq_FF, 0xFF
