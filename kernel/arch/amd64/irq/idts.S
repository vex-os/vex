.section .text

.macro push_interrupt_frame
    pushq %r15                          # hart_context::amd64_r15
    pushq %r14                          # hart_context::amd64_r14
    pushq %r13                          # hart_context::amd64_r13
    pushq %r12                          # hart_context::amd64_r12
    pushq %r11                          # hart_context::amd64_r11
    pushq %r10                          # hart_context::amd64_r10
    pushq %r9                           # hart_context::amd64_r9
    pushq %r8                           # hart_context::amd64_r8
    pushq %rbp                          # hart_context::amd64_rbp
    pushq %rdi                          # hart_context::amd64_rdi
    pushq %rsi                          # hart_context::amd64_rsi
    pushq %rdx                          # hart_context::amd64_rdx
    pushq %rcx                          # hart_context::amd64_rcx
    pushq %rbx                          # hart_context::amd64_rbx
    pushq %rax                          # hart_context::amd64_rax
    movq %es, %rax
    pushq %rax                          # hart_context::amd64_es
    movq %ds, %rax
    pushq %rax                          # hart_context::amd64_ds
.endm

.macro pop_interrupt_frame
    popq %rax
    movq %rax, %ds                      # hart_context::amd64_ds
    popq %rax
    movq %rax, %es                      # hart_context::amd64_es
    popq %rax                           # hart_context::amd64_rax
    popq %rbx                           # hart_context::amd64_rbx
    popq %rcx                           # hart_context::amd64_rcx
    popq %rdx                           # hart_context::amd64_rdx
    popq %rsi                           # hart_context::amd64_rsi
    popq %rdi                           # hart_context::amd64_rdi
    popq %rbp                           # hart_context::amd64_rbp
    popq %r8                            # hart_context::amd64_r8
    popq %r9                            # hart_context::amd64_r9
    popq %r10                           # hart_context::amd64_r10
    popq %r11                           # hart_context::amd64_r11
    popq %r12                           # hart_context::amd64_r12
    popq %r13                           # hart_context::amd64_r13
    popq %r14                           # hart_context::amd64_r14
    popq %r15                           # hart_context::amd64_r15
.endm

.macro swapgs_if_user
    testq $3, 16(%rsp)                  # hart_context::amd64_cs RPL bits
    jz 1f
    swapgs
    1:
.endm

.macro return_from_stub
    testq $3, 152(%rsp)                 # hart_context::amd64_cs RPL bits
    jnz amd64_return_to_user
    pop_interrupt_frame
    addq $0x08, %rsp
    iretq
.endm

.global amd64_return_to_user
.type amd64_return_to_user, @function
amd64_return_to_user:
    movq %rsp, %rdi
    call exit_to_user

    pop_interrupt_frame
    swapgs
    addq $0x08, %rsp
    iretq

.macro isr_stub stubname vector
    .global \stubname
    .type \stubname, @function
    \stubname:
        cli
        cld                             # Ring 3 may leave DF set, C code expects it clear
        swapgs_if_user
        push_interrupt_frame

        movq %rsp, %rdi
        movq $\vector, %rsi
        call amd64_isr_handler

        return_from_stub
.endm

.macro isr_stub_pz stubname vector
    .global \stubname
    .type \stubname, @function
    \stubname:
        cli
        cld
        pushq $0x0000000000000000       # hart_context::amd64_err
        swapgs_if_user
        push_interrupt_frame

        movq %rsp, %rdi
        movq $\vector, %rsi
        call amd64_isr_handler

        return_from_stub
.endm

# NMI, #DF and #MC can arrive anywhere, including the window
# between SYSCALL and its SWAPGS, where CS is already the kernel's
# but GS is still the user's; #NP, #SS and #GP also come from an
# IRETQ back to ring 3 that faults after its SWAPGS, with the kernel's
# CS in the frame; GS_BASE itself tells which one is live: kernel
# thread pointers are higher-half, so the MSR's sign bit is set
.macro isr_stub_paranoid stubname vector has_errcode user_return
    .global \stubname
    .type \stubname, @function
    \stubname:
        cld
        .if \has_errcode == 0
        pushq $0x0000000000000000       # hart_context::amd64_err
        .endif
        push_interrupt_frame

        xorl %ebx, %ebx
        movl $0xC0000101, %ecx          # MSR_GS_BASE
        rdmsr
        testl %edx, %edx
        js 1f
        swapgs
        movl $1, %ebx
    1:
        movq %rsp, %rdi
        movq $\vector, %rsi
        call amd64_isr_handler

        .if \user_return
        testq $3, 152(%rsp)             # hart_context::amd64_cs RPL bits
        jnz amd64_return_to_user
        .endif

        testl %ebx, %ebx
        jz 2f
        swapgs
    2:
        pop_interrupt_frame
        addq $0x08, %rsp
        iretq
.endm

.macro spur_stub stubname vector
    .global \stubname
    .type \stubname, @function
    \stubname:
        cli
        cld
        pushq $0x0000000000000000
        swapgs_if_user
        push_interrupt_frame

        movq %rsp, %rdi
        movq $\vector, %rsi
        call amd64_spur_handler

        pop_interrupt_frame
        swapgs_if_user
        addq $0x08, %rsp
        iretq
.endm

.macro irq_stub stubname vector
    .global \stubname
    .type \stubname, @function
    \stubname:
        cli
        cld
        pushq $0x0000000000000000
        swapgs_if_user
        push_interrupt_frame

        movq %rsp, %rdi
        movq $\vector, %rsi
        call amd64_irq_handler

        return_from_stub
.endm

isr_stub_pz         amd64_isr_00, 0x00
isr_stub_pz         amd64_isr_01, 0x01
isr_stub_paranoid   amd64_isr_02, 0x02, 0, 0
isr_stub_pz         amd64_isr_03, 0x03
isr_stub_pz         amd64_isr_04, 0x04
isr_stub_pz         amd64_isr_05, 0x05
isr_stub_pz         amd64_isr_06, 0x06
isr_stub_pz         amd64_isr_07, 0x07
isr_stub_paranoid   amd64_isr_08, 0x08, 1, 0
isr_stub_pz         amd64_isr_09, 0x09
isr_stub            amd64_isr_0A, 0x0A
isr_stub_paranoid   amd64_isr_0B, 0x0B, 1, 1
isr_stub_paranoid   amd64_isr_0C, 0x0C, 1, 1
isr_stub_paranoid   amd64_isr_0D, 0x0D, 1, 1
isr_stub            amd64_isr_0E, 0x0E
isr_stub_pz         amd64_isr_0F, 0x0F
isr_stub_pz         amd64_isr_10, 0x10
isr_stub            amd64_isr_11, 0x11
isr_stub_paranoid   amd64_isr_12, 0x12, 0, 0
isr_stub_pz         amd64_isr_13, 0x13
isr_stub_pz         amd64_isr_14, 0x14
isr_stub            amd64_isr_15, 0x15
isr_stub_pz         amd64_isr_16, 0x16
isr_stub_pz         amd64_isr_17, 0x17
isr_stub_pz         amd64_isr_18, 0x18
isr_stub_pz         amd64_isr_19, 0x19
isr_stub_pz         amd64_isr_1A, 0x1A
isr_stub_pz         amd64_isr_1B, 0x1B
isr_stub_pz         amd64_isr_1C, 0x1C
isr_stub_pz         amd64_isr_1D, 0x1D
isr_stub_pz         amd64_isr_1E, 0x1E
isr_stub_pz         amd64_isr_1F, 0x1F

spur_stub amd64_spur_20, 0x20
spur_stub amd64_spur_21, 0x21
spur_stub amd64_spur_22, 0x22
spur_stub amd64_spur_23, 0x23
spur_stub amd64_spur_24, 0x24
spur_stub amd64_spur_25, 0x25
spur_stub amd64_spur_26, 0x26
spur_stub amd64_spur_27, 0x27
spur_stub amd64_spur_28, 0x28
spur_stub amd64_spur_29, 0x29
spur_stub amd64_spur_2A, 0x2A
spur_stub amd64_spur_2B, 0x2B
spur_stub amd64_spur_2C, 0x2C
spur_stub amd64_spur_2D, 0x2D
spur_stub amd64_spur_2E, 0x2E
spur_stub amd64_spur_2F, 0x2F

irq_stub amd64_irq_30, 0x30
irq_stub amd64_irq_31, 0x31
irq_stub amd64_irq_32, 0x32
irq_stub amd64_irq_33, 0x33
irq_stub amd64_irq_34, 0x34
irq_stub amd64_irq_35, 0x35
irq_stub amd64_irq_36, 0x36
irq_stub amd64_irq_37, 0x37
irq_stub amd64_irq_38, 0x38
irq_stub amd64_irq_39, 0x39
irq_stub amd64_irq_3A, 0x3A
irq_stub amd64_irq_3B, 0x3B
irq_stub amd64_irq_3C, 0x3C
irq_stub amd64_irq_3D, 0x3D
irq_stub amd64_irq_3E, 0x3E
irq_stub amd64_irq_3F, 0x3F
irq_stub amd64_irq_40, 0x40
irq_stub amd64_irq_41, 0x41
irq_stub amd64_irq_42, 0x42
irq_stub amd64_irq_43, 0x43
irq_stub amd64_irq_44, 0x44
irq_stub amd64_irq_45, 0x45
irq_stub amd64_irq_46, 0x46
irq_stub amd64_irq_47, 0x47
irq_stub amd64_irq_48, 0x48
irq_stub amd64_irq_49, 0x49
irq_stub amd64_irq_4A, 0x4A
irq_stub amd64_irq_4B, 0x4B
irq_stub amd64_irq_4C, 0x4C
irq_stub amd64_irq_4D, 0x4D
irq_stub amd64_irq_4E, 0x4E
irq_stub amd64_irq_4F, 0x4F
irq_stub amd64_irq_50, 0x50
irq_stub amd64_irq_51, 0x51
irq_stub amd64_irq_52, 0x52
irq_stub amd64_irq_53, 0x53
irq_stub amd64_irq_54, 0x54
irq_stub amd64_irq_55, 0x55
irq_stub amd64_irq_56, 0x56
irq_stub amd64_irq_57, 0x57
irq_stub amd64_irq_58, 0x58
irq_stub amd64_irq_59, 0x59
irq_stub amd64_irq_5A, 0x5A
irq_stub amd64_irq_5B, 0x5B
irq_stub amd64_irq_5C, 0x5C
irq_stub amd64_irq_5D, 0x5D
irq_stub amd64_irq_5E, 0x5E
irq_stub amd64_irq_5F, 0x5F
irq_stub amd64_irq_60, 0x60
irq_stub amd64_irq_61, 0x61
irq_stub amd64_irq_62, 0x62
irq_stub amd64_irq_63, 0x63
irq_stub amd64_irq_64, 0x64
irq_stub amd64_irq_65, 0x65
irq_stub amd64_irq_66, 0x66
irq_stub amd64_irq_67, 0x67
irq_stub amd64_irq_68, 0x68
irq_stub amd64_irq_69, 0x69
irq_stub amd64_irq_6A, 0x6A
irq_stub amd64_irq_6B, 0x6B
irq_stub amd64_irq_6C, 0x6C
irq_stub amd64_irq_6D, 0x6D
irq_stub amd64_irq_6E, 0x6E
irq_stub amd64_irq_6F, 0x6F
irq_stub amd64_irq_70, 0x70
irq_stub amd64_irq_71, 0x71
irq_stub amd64_irq_72, 0x72
irq_stub amd64_irq_73, 0x73
irq_stub amd64_irq_74, 0x74
irq_stub amd64_irq_75, 0x75
irq_stub amd64_irq_76, 0x76
irq_stub amd64_irq_77, 0x77
irq_stub amd64_irq_78, 0x78
irq_stub amd64_irq_79, 0x79
irq_stub amd64_irq_7A, 0x7A
irq_stub amd64_irq_7B, 0x7B
irq_stub amd64_irq_7C, 0x7C
irq_stub amd64_irq_7D, 0x7D
irq_stub amd64_irq_7E, 0x7E
irq_stub amd64_irq_7F, 0x7F
irq_stub amd64_irq_80, 0x80
irq_stub amd64_irq_81, 0x81
irq_stub amd64_irq_82, 0x82
irq_stub amd64_irq_83, 0x83
irq_stub amd64_irq_84, 0x84
irq_stub amd64_irq_85, 0x85
irq_stub amd64_irq_86, 0x86
irq_stub amd64_irq_87, 0x87
irq_stub amd64_irq_88, 0x88
irq_stub amd64_irq_89, 0x89
irq_stub amd64_irq_8A, 0x8A
irq_stub amd64_irq_8B, 0x8B
irq_stub amd64_irq_8C, 0x8C
irq_stub amd64_irq_8D, 0x8D
irq_stub amd64_irq_8E, 0x8E
irq_stub amd64_irq_8F, 0x8F
irq_stub amd64_irq_90, 0x90
irq_stub amd64_irq_91, 0x91
irq_stub amd64_irq_92, 0x92
irq_stub amd64_irq_93, 0x93
irq_stub amd64_irq_94, 0x94
irq_stub amd64_irq_95, 0x95
irq_stub amd64_irq_96, 0x96
irq_stub amd64_irq_97, 0x97
irq_stub amd64_irq_98, 0x98
irq_stub amd64_irq_99, 0x99
irq_stub amd64_irq_9A, 0x9A
irq_stub amd64_irq_9B, 0x9B
irq_stub amd64_irq_9C, 0x9C
irq_stub amd64_irq_9D, 0x9D
irq_stub amd64_irq_9E, 0x9E
irq_stub amd64_irq_9F, 0x9F
irq_stub amd64_irq_A0, 0xA0
irq_stub amd64_irq_A1, 0xA1
irq_stub amd64_irq_A2, 0xA2
irq_stub amd64_irq_A3, 0xA3
irq_stub amd64_irq_A4, 0xA4
irq_stub amd64_irq_A5, 0xA5
irq_stub amd64_irq_A6, 0xA6
irq_stub amd64_irq_A7, 0xA7
irq_stub amd64_irq_A8, 0xA8
irq_stub amd64_irq_A9, 0xA9
irq_stub amd64_irq_AA, 0xAA
irq_stub amd64_irq_AB, 0xAB
irq_stub amd64_irq_AC, 0xAC
irq_stub amd64_irq_AD, 0xAD
irq_stub amd64_irq_AE, 0xAE
irq_stub amd64_irq_AF, 0xAF
irq_stub amd64_irq_B0, 0xB0
irq_stub amd64_irq_B1, 0xB1
irq_stub amd64_irq_B2, 0xB2
irq_stub amd64_irq_B3, 0xB3
irq_stub amd64_irq_B4, 0xB4
irq_stub amd64_irq_B5, 0xB5
irq_stub amd64_irq_B6, 0xB6
irq_stub amd64_irq_B7, 0xB7
irq_stub amd64_irq_B8, 0xB8
irq_stub amd64_irq_B9, 0xB9
irq_stub amd64_irq_BA, 0xBA
irq_stub amd64_irq_BB, 0xBB
irq_stub amd64_irq_BC, 0xBC
irq_stub amd64_irq_BD, 0xBD
irq_stub amd64_irq_BE, 0xBE
irq_stub amd64_irq_BF, 0xBF
irq_stub amd64_irq_C0, 0xC0
irq_stub amd64_irq_C1, 0xC1
irq_stub amd64_irq_C2, 0xC2
irq_stub amd64_irq_C3, 0xC3
irq_stub amd64_irq_C4, 0xC4
irq_stub amd64_irq_C5, 0xC5
irq_stub amd64_irq_C6, 0xC6
irq_stub amd64_irq_C7, 0xC7
irq_stub amd64_irq_C8, 0xC8
irq_stub amd64_irq_C9, 0xC9
irq_stub amd64_irq_CA, 0xCA
irq_stub amd64_irq_CB, 0xCB
irq_stub amd64_irq_CC, 0xCC
irq_stub amd64_irq_CD, 0xCD
irq_stub amd64_irq_CE, 0xCE
irq_stub amd64_irq_CF, 0xCF
irq_stub amd64_irq_D0, 0xD0
irq_stub amd64_irq_D1, 0xD1
irq_stub amd64_irq_D2, 0xD2
irq_stub amd64_irq_D3, 0xD3
irq_stub amd64_irq_D4, 0xD4
irq_stub amd64_irq_D5, 0xD5
irq_stub amd64_irq_D6, 0xD6
irq_stub amd64_irq_D7, 0xD7
irq_stub amd64_irq_D8, 0xD8
irq_stub amd64_irq_D9, 0xD9
irq_stub amd64_irq_DA, 0xDA
irq_stub amd64_irq_DB, 0xDB
irq_stub amd64_irq_DC, 0xDC
irq_stub amd64_irq_DD, 0xDD
irq_stub amd64_irq_DE, 0xDE
irq_stub amd64_irq_DF, 0xDF
irq_stub amd64_irq_E0, 0xE0
irq_stub amd64_irq_E1, 0xE1
irq_stub amd64_irq_E2, 0xE2
irq_stub amd64_irq_E3, 0xE3
irq_stub amd64_irq_E4, 0xE4
irq_stub amd64_irq_E5, 0xE5
irq_stub amd64_irq_E6, 0xE6
irq_stub amd64_irq_E7, 0xE7
irq_stub amd64_irq_E8, 0xE8
irq_stub amd64_irq_E9, 0xE9
irq_stub amd64_irq_EA, 0xEA
irq_stub amd64_irq_EB, 0xEB
irq_stub amd64_irq_EC, 0xEC
irq_stub amd64_irq_ED, 0xED
irq_stub amd64_irq_EE, 0xEE
irq_stub amd64_irq_EF, 0xEF
irq_stub amd64_irq_F0, 0xF0
irq_stub amd64_irq_F1, 0xF1
irq_stub amd64_irq_F2, 0xF2
irq_stub amd64_irq_F3, 0xF3
irq_stub amd64_irq_F4, 0xF4
irq_stub amd64_irq_F5, 0xF5
irq_stub amd64_irq_F6, 0xF6
irq_stub amd64_irq_F7, 0xF7
irq_stub amd64_irq_F8, 0xF8
irq_stub amd64_irq_F9, 0xF9
irq_stub amd64_irq_FA, 0xFA
irq_stub amd64_irq_FB, 0xFB
irq_stub amd64_irq_FC, 0xFC
irq_stub amd64_irq_FD, 0xFD
irq_stub amd64_irq_FE, 0xFE
irq_stub amd64_irq_FF, 0xFF
