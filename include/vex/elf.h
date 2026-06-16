#ifndef A5B67F05_8F06_424F_9A95_ABEEBF66D8F9
#define A5B67F05_8F06_424F_9A95_ABEEBF66D8F9
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define EI_NIDENT 16

#define ET_NONE   UINT16_C(0)      // No file type
#define ET_REL    UINT16_C(1)      // Relocatable file
#define ET_EXEC   UINT16_C(2)      // Executable file
#define ET_DYN    UINT16_C(3)      // Shared object file
#define ET_CORE   UINT16_C(4)      // Core file
#define ET_NUM    UINT16_C(5)      // Number of defined types
#define ET_LOOS   UINT16_C(0xFE00) // OS-specific range start
#define ET_HIOS   UINT16_C(0xFEFF) // OS-specific range end
#define ET_LOPROC UINT16_C(0xFF00) // Processor-specific range start
#define ET_HIPROC UINT16_C(0xFFFF) // Processor-specific range end

#define EM_NONE          UINT16_C(0)   // No machine
#define EM_M32           UINT16_C(1)   // AT&T WE 32100
#define EM_SPARC         UINT16_C(2)   // SUN SPARC
#define EM_386           UINT16_C(3)   // Intel 80386
#define EM_68K           UINT16_C(4)   // Motorola m68k family
#define EM_88K           UINT16_C(5)   // Motorola m88k family
#define EM_IAMCU         UINT16_C(6)   // Intel MCU
#define EM_860           UINT16_C(7)   // Intel 80860
#define EM_MIPS          UINT16_C(8)   // MIPS R3000 big-endian
#define EM_S370          UINT16_C(9)   // IBM System/370
#define EM_MIPS_RS3_LE   UINT16_C(10)  // MIPS R3000 little-endian
#define EM_PARISC        UINT16_C(15)  // HPPA
#define EM_VPP500        UINT16_C(17)  // Fujitsu VPP500
#define EM_SPARC32PLUS   UINT16_C(18)  // Sun's "v8plus"
#define EM_960           UINT16_C(19)  // Intel 80960
#define EM_PPC           UINT16_C(20)  // PowerPC
#define EM_PPC64         UINT16_C(21)  // PowerPC 64-bit
#define EM_S390          UINT16_C(22)  // IBM S390
#define EM_SPU           UINT16_C(23)  // IBM SPU/SPC
#define EM_V800          UINT16_C(36)  // NEC V800 series
#define EM_FR20          UINT16_C(37)  // Fujitsu FR20
#define EM_RH32          UINT16_C(38)  // TRW RH-32
#define EM_RCE           UINT16_C(39)  // Motorola RCE
#define EM_ARM           UINT16_C(40)  // ARM
#define EM_FAKE_ALPHA    UINT16_C(41)  // Digital Alpha
#define EM_SH            UINT16_C(42)  // Hitachi SH
#define EM_SPARCV9       UINT16_C(43)  // SPARC v9 64-bit
#define EM_TRICORE       UINT16_C(44)  // Siemens Tricore
#define EM_ARC           UINT16_C(45)  // Argonaut RISC Core
#define EM_H8_300        UINT16_C(46)  // Hitachi H8/300
#define EM_H8_300H       UINT16_C(47)  // Hitachi H8/300H
#define EM_H8S           UINT16_C(48)  // Hitachi H8S
#define EM_H8_500        UINT16_C(49)  // Hitachi H8/500
#define EM_IA_64         UINT16_C(50)  // Intel Merced
#define EM_MIPS_X        UINT16_C(51)  // Stanford MIPS-X
#define EM_COLDFIRE      UINT16_C(52)  // Motorola Coldfire
#define EM_68HC12        UINT16_C(53)  // Motorola M68HC12
#define EM_MMA           UINT16_C(54)  // Fujitsu MMA Multimedia Accelerator
#define EM_PCP           UINT16_C(55)  // Siemens PCP
#define EM_NCPU          UINT16_C(56)  // Sony nCPU embeeded RISC
#define EM_NDR1          UINT16_C(57)  // Denso NDR1 microprocessor
#define EM_STARCORE      UINT16_C(58)  // Motorola Start*Core processor
#define EM_ME16          UINT16_C(59)  // Toyota ME16 processor
#define EM_ST100         UINT16_C(60)  // STMicroelectronic ST100 processor
#define EM_TINYJ         UINT16_C(61)  // Advanced Logic Corp. Tinyj emb.fam
#define EM_X86_64        UINT16_C(62)  // AMD x86-64 architecture
#define EM_PDSP          UINT16_C(63)  // Sony DSP Processor
#define EM_PDP10         UINT16_C(64)  // Digital PDP-10
#define EM_PDP11         UINT16_C(65)  // Digital PDP-11
#define EM_FX66          UINT16_C(66)  // Siemens FX66 microcontroller
#define EM_ST9PLUS       UINT16_C(67)  // STMicroelectronics ST9+ 8/16 mc
#define EM_ST7           UINT16_C(68)  // STmicroelectronics ST7 8 bit mc
#define EM_68HC16        UINT16_C(69)  // Motorola MC68HC16 microcontroller
#define EM_68HC11        UINT16_C(70)  // Motorola MC68HC11 microcontroller
#define EM_68HC08        UINT16_C(71)  // Motorola MC68HC08 microcontroller
#define EM_68HC05        UINT16_C(72)  // Motorola MC68HC05 microcontroller
#define EM_SVX           UINT16_C(73)  // Silicon Graphics SVx
#define EM_ST19          UINT16_C(74)  // STMicroelectronics ST19 8 bit mc
#define EM_VAX           UINT16_C(75)  // Digital VAX
#define EM_CRIS          UINT16_C(76)  // Axis Communications 32-bit emb.proc
#define EM_JAVELIN       UINT16_C(77)  // Infineon Technologies 32-bit emb.proc
#define EM_FIREPATH      UINT16_C(78)  // Element 14 64-bit DSP Processor
#define EM_ZSP           UINT16_C(79)  // LSI Logic 16-bit DSP Processor
#define EM_MMIX          UINT16_C(80)  // Donald Knuth's educational 64-bit proc
#define EM_HUANY         UINT16_C(81)  // Harvard University machine-independent object files
#define EM_PRISM         UINT16_C(82)  // SiTera Prism
#define EM_AVR           UINT16_C(83)  // Atmel AVR 8-bit microcontroller
#define EM_FR30          UINT16_C(84)  // Fujitsu FR30
#define EM_D10V          UINT16_C(85)  // Mitsubishi D10V
#define EM_D30V          UINT16_C(86)  // Mitsubishi D30V
#define EM_V850          UINT16_C(87)  // NEC v850
#define EM_M32R          UINT16_C(88)  // Mitsubishi M32R
#define EM_MN10300       UINT16_C(89)  // Matsushita MN10300
#define EM_MN10200       UINT16_C(90)  // Matsushita MN10200
#define EM_PJ            UINT16_C(91)  // picoJava
#define EM_OPENRISC      UINT16_C(92)  // OpenRISC 32-bit embedded processor
#define EM_ARC_COMPACT   UINT16_C(93)  // ARC International ARCompact
#define EM_XTENSA        UINT16_C(94)  // Tensilica Xtensa Architecture
#define EM_VIDEOCORE     UINT16_C(95)  // Alphamosaic VideoCore
#define EM_TMM_GPP       UINT16_C(96)  // Thompson Multimedia General Purpose Proc
#define EM_NS32K         UINT16_C(97)  // National Semi. 32000
#define EM_TPC           UINT16_C(98)  // Tenor Network TPC
#define EM_SNP1K         UINT16_C(99)  // Trebia SNP 1000
#define EM_ST200         UINT16_C(100) // STMicroelectronics ST200
#define EM_IP2K          UINT16_C(101) // Ubicom IP2xxx
#define EM_MAX           UINT16_C(102) // MAX processor
#define EM_CR            UINT16_C(103) // National Semi. CompactRISC
#define EM_F2MC16        UINT16_C(104) // Fujitsu F2MC16
#define EM_MSP430        UINT16_C(105) // Texas Instruments msp430
#define EM_BLACKFIN      UINT16_C(106) // Analog Devices Blackfin DSP
#define EM_SE_C33        UINT16_C(107) // Seiko Epson S1C33 family
#define EM_SEP           UINT16_C(108) // Sharp embedded microprocessor
#define EM_ARCA          UINT16_C(109) // Arca RISC
#define EM_UNICORE       UINT16_C(110) // PKU-Unity & MPRC Peking Uni. mc series
#define EM_EXCESS        UINT16_C(111) // eXcess configurable cpu
#define EM_DXP           UINT16_C(112) // Icera Semi. Deep Execution Processor
#define EM_ALTERA_NIOS2  UINT16_C(113) // Altera Nios II
#define EM_CRX           UINT16_C(114) // National Semi. CompactRISC CRX
#define EM_XGATE         UINT16_C(115) // Motorola XGATE
#define EM_C166          UINT16_C(116) // Infineon C16x/XC16x
#define EM_M16C          UINT16_C(117) // Renesas M16C
#define EM_DSPIC30F      UINT16_C(118) // Microchip Technology dsPIC30F
#define EM_CE            UINT16_C(119) // Freescale Communication Engine RISC
#define EM_M32C          UINT16_C(120) // Renesas M32C
#define EM_TSK3000       UINT16_C(131) // Altium TSK3000
#define EM_RS08          UINT16_C(132) // Freescale RS08
#define EM_SHARC         UINT16_C(133) // Analog Devices SHARC family
#define EM_ECOG2         UINT16_C(134) // Cyan Technology eCOG2
#define EM_SCORE7        UINT16_C(135) // Sunplus S+core7 RISC
#define EM_DSP24         UINT16_C(136) // New Japan Radio (NJR) 24-bit DSP
#define EM_VIDEOCORE3    UINT16_C(137) // Broadcom VideoCore III
#define EM_LATTICEMICO32 UINT16_C(138) // RISC for Lattice FPGA
#define EM_SE_C17        UINT16_C(139) // Seiko Epson C17
#define EM_TI_C6000      UINT16_C(140) // Texas Instruments TMS320C6000 DSP
#define EM_TI_C2000      UINT16_C(141) // Texas Instruments TMS320C2000 DSP
#define EM_TI_C5500      UINT16_C(142) // Texas Instruments TMS320C55x DSP
#define EM_TI_ARP32      UINT16_C(143) // Texas Instruments App. Specific RISC
#define EM_TI_PRU        UINT16_C(144) // Texas Instruments Prog. Realtime Unit
#define EM_MMDSP_PLUS    UINT16_C(160) // STMicroelectronics 64bit VLIW DSP
#define EM_CYPRESS_M8C   UINT16_C(161) // Cypress M8C
#define EM_R32C          UINT16_C(162) // Renesas R32C
#define EM_TRIMEDIA      UINT16_C(163) // NXP Semi. TriMedia
#define EM_QDSP6         UINT16_C(164) // QUALCOMM DSP6
#define EM_8051          UINT16_C(165) // Intel 8051 and variants
#define EM_STXP7X        UINT16_C(166) // STMicroelectronics STxP7x
#define EM_NDS32         UINT16_C(167) // Andes Tech. compact code emb. RISC
#define EM_ECOG1X        UINT16_C(168) // Cyan Technology eCOG1X
#define EM_MAXQ30        UINT16_C(169) // Dallas Semi. MAXQ30 mc
#define EM_XIMO16        UINT16_C(170) // New Japan Radio (NJR) 16-bit DSP
#define EM_MANIK         UINT16_C(171) // M2000 Reconfigurable RISC
#define EM_CRAYNV2       UINT16_C(172) // Cray NV2 vector architecture
#define EM_RX            UINT16_C(173) // Renesas RX
#define EM_METAG         UINT16_C(174) // Imagination Tech. META
#define EM_MCST_ELBRUS   UINT16_C(175) // MCST Elbrus
#define EM_ECOG16        UINT16_C(176) // Cyan Technology eCOG16
#define EM_CR16          UINT16_C(177) // National Semi. CompactRISC CR16
#define EM_ETPU          UINT16_C(178) // Freescale Extended Time Processing Unit
#define EM_SLE9X         UINT16_C(179) // Infineon Tech. SLE9X
#define EM_L10M          UINT16_C(180) // Intel L10M
#define EM_K10M          UINT16_C(181) // Intel K10M
#define EM_AARCH64       UINT16_C(183) // ARM AARCH64
#define EM_AVR32         UINT16_C(185) // Amtel 32-bit microprocessor
#define EM_STM8          UINT16_C(186) // STMicroelectronics STM8
#define EM_TILE64        UINT16_C(187) // Tileta TILE64
#define EM_TILEPRO       UINT16_C(188) // Tilera TILEPro
#define EM_MICROBLAZE    UINT16_C(189) // Xilinx MicroBlaze
#define EM_CUDA          UINT16_C(190) // NVIDIA CUDA
#define EM_TILEGX        UINT16_C(191) // Tilera TILE-Gx
#define EM_CLOUDSHIELD   UINT16_C(192) // CloudShield
#define EM_COREA_1ST     UINT16_C(193) // KIPO-KAIST Core-A 1st gen.
#define EM_COREA_2ND     UINT16_C(194) // KIPO-KAIST Core-A 2nd gen.
#define EM_ARC_COMPACT2  UINT16_C(195) // Synopsys ARCompact V2
#define EM_OPEN8         UINT16_C(196) // Open8 RISC
#define EM_RL78          UINT16_C(197) // Renesas RL78
#define EM_VIDEOCORE5    UINT16_C(198) // Broadcom VideoCore V
#define EM_78KOR         UINT16_C(199) // Renesas 78KOR
#define EM_56800EX       UINT16_C(200) // Freescale 56800EX DSC
#define EM_BA1           UINT16_C(201) // Beyond BA1
#define EM_BA2           UINT16_C(202) // Beyond BA2
#define EM_XCORE         UINT16_C(203) // XMOS xCORE
#define EM_MCHP_PIC      UINT16_C(204) // Microchip 8-bit PIC(r)
#define EM_KM32          UINT16_C(210) // KM211 KM32
#define EM_KMX32         UINT16_C(211) // KM211 KMX32
#define EM_EMX16         UINT16_C(212) // KM211 KMX16
#define EM_EMX8          UINT16_C(213) // KM211 KMX8
#define EM_KVARC         UINT16_C(214) // KM211 KVARC
#define EM_CDP           UINT16_C(215) // Paneve CDP
#define EM_COGE          UINT16_C(216) // Cognitive Smart Memory Processor
#define EM_COOL          UINT16_C(217) // Bluechip CoolEngine
#define EM_NORC          UINT16_C(218) // Nanoradio Optimized RISC
#define EM_CSR_KALIMBA   UINT16_C(219) // CSR Kalimba
#define EM_Z80           UINT16_C(220) // Zilog Z80
#define EM_VISIUM        UINT16_C(221) // Controls and Data Services VISIUMcore
#define EM_FT32          UINT16_C(222) // FTDI Chip FT32
#define EM_MOXIE         UINT16_C(223) // Moxie processor
#define EM_AMDGPU        UINT16_C(224) // AMD GPU
#define EM_RISCV         UINT16_C(243) // RISC-V
#define EM_BPF           UINT16_C(247) // Linux BPF -- in-kernel virtual machine
#define EM_CSKY          UINT16_C(252) // C-SKY
#define EM_NUM           UINT16_C(253)

#define EV_NONE    UINT32_C(0) // Invalid ELF version
#define EV_CURRENT UINT32_C(1) // Current version
#define EV_NUM     UINT32_C(2)

struct elf32_ehdr {
    uint8_t e_ident[EI_NIDENT]; // Magic number and other info
    uint16_t e_type;            // Object file type
    uint16_t e_machine;         // Architecture
    uint32_t e_version;         // Object file version
    uint32_t e_entry;           // Entry point virtual address
    uint32_t e_phoff;           // Program header table file offset
    uint32_t e_shoff;           // Section header table file offset
    uint32_t e_flags;           // Processor-specific flags
    uint16_t e_ehsize;          // ELF header size in bytes
    uint16_t e_phentsize;       // Program header table entry size
    uint16_t e_phnum;           // Program header table entry count
    uint16_t e_shentsize;       // Section header table entry size
    uint16_t e_shnum;           // Section header table entry count
    uint16_t e_shstrndx;        // Section header string table index
} __packed;

struct elf64_ehdr {
    uint8_t e_ident[EI_NIDENT]; // Magic number and other info
    uint16_t e_type;            // Object file type
    uint16_t e_machine;         // Architecture
    uint32_t e_version;         // Object file version
    uint64_t e_entry;           // Entry point virtual address
    uint64_t e_phoff;           // Program header table file offset
    uint64_t e_shoff;           // Section header table file offset
    uint32_t e_flags;           // Processor-specific flags
    uint16_t e_ehsize;          // ELF header size in bytes
    uint16_t e_phentsize;       // Program header table entry size
    uint16_t e_phnum;           // Program header table entry count
    uint16_t e_shentsize;       // Section header table entry size
    uint16_t e_shnum;           // Section header table entry count
    uint16_t e_shstrndx;        // Section header string table index
} __packed;

#define SHT_NULL           UINT32_C(0)          // Section header table entry unused
#define SHT_PROGBITS       UINT32_C(1)          // Program data
#define SHT_SYMTAB         UINT32_C(2)          // Symbol table
#define SHT_STRTAB         UINT32_C(3)          // String table
#define SHT_RELA           UINT32_C(4)          // Relocation entries with addends
#define SHT_HASH           UINT32_C(5)          // Symbol hash table
#define SHT_DYNAMIC        UINT32_C(6)          // Dynamic linking information
#define SHT_NOTE           UINT32_C(7)          // Notes
#define SHT_NOBITS         UINT32_C(8)          // Program space with no data (bss)
#define SHT_REL            UINT32_C(9)          // Relocation entries, no addends
#define SHT_SHLIB          UINT32_C(10)         // Reserved
#define SHT_DYNSYM         UINT32_C(11)         // Dynamic linker symbol table
#define SHT_INIT_ARRAY     UINT32_C(14)         // Array of constructors
#define SHT_FINI_ARRAY     UINT32_C(15)         // Array of destructors
#define SHT_PREINIT_ARRAY  UINT32_C(16)         // Array of pre-constructors
#define SHT_GROUP          UINT32_C(17)         // Section group
#define SHT_SYMTAB_SHNDX   UINT32_C(18)         // Extended section indeces
#define SHT_NUM            UINT32_C(19)         // Number of defined types.
#define SHT_LOOS           UINT32_C(0x60000000) // Start OS-specific.
#define SHT_GNU_ATTRIBUTES UINT32_C(0x6FFFFFF5) // Object attributes.
#define SHT_GNU_HASH       UINT32_C(0x6FFFFFF6) // GNU-style hash table.
#define SHT_GNU_LIBLIST    UINT32_C(0x6FFFFFF7) // Prelink library list
#define SHT_CHECKSUM       UINT32_C(0x6FFFFFF8) // Checksum for DSO content.
#define SHT_LOSUNW         UINT32_C(0x6FFFFFFA) // Sun-specific low bound.
#define SHT_SUNW_move      UINT32_C(0x6FFFFFFA)
#define SHT_SUNW_COMDAT    UINT32_C(0x6FFFFFFB)
#define SHT_SUNW_syminfo   UINT32_C(0x6FFFFFFC)
#define SHT_GNU_verdef     UINT32_C(0x6FFFFFFD) // Version definition section.
#define SHT_GNU_verneed    UINT32_C(0x6FFFFFFE) // Version needs section.
#define SHT_GNU_versym     UINT32_C(0x6FFFFFFF) // Version symbol table.
#define SHT_HISUNW         UINT32_C(0x6FFFFFFF) // Sun-specific high bound.
#define SHT_HIOS           UINT32_C(0x6FFFFFFF) // End OS-specific type
#define SHT_LOPROC         UINT32_C(0x70000000) // Start of processor-specific
#define SHT_HIPROC         UINT32_C(0x7FFFFFFF) // End of processor-specific
#define SHT_LOUSER         UINT32_C(0x80000000) // Start of application-specific
#define SHT_HIUSER         UINT32_C(0x8FFFFFFF) // End of application-specific

#define SHF_WRITE            0x00000001 // Writable
#define SHF_ALLOC            0x00000002 // Occupies memory during execution
#define SHF_EXECINSTR        0x00000004 // Executable
#define SHF_MERGE            0x00000010 // Might be merged
#define SHF_STRINGS          0x00000020 // Contains nul-terminated strings
#define SHF_INFO_LINK        0x00000040 // `sh_info' contains SHT index
#define SHF_LINK_ORDER       0x00000080 // Preserve order after combining
#define SHF_OS_NONCONFORMING 0x00000100 // Non-standard OS specific handling required
#define SHF_GROUP            0x00000200 // Section is member of a group.
#define SHF_TLS              0x00000400 // Section hold thread-local data.
#define SHF_COMPRESSED       0x00000800 // Section with compressed data.
#define SHF_MASKOS           0x0FF00000 // OS-specific.
#define SHF_MASKPROC         0xF0000000 // Processor-specific
#define SHF_ORDERED          0x40000000 // Special ordering requirement (Solaris).
#define SHF_EXCLUDE          0x80000000 // Section is excluded unless referenced or allocated (Solaris).

struct elf32_shdr {
    uint32_t sh_name;      // Section name (string table index)
    uint32_t sh_type;      // Section type
    uint32_t sh_flags;     // Section flags
    uint32_t sh_addr;      // Section virtual address at execution
    uint32_t sh_offset;    // Section file offset
    uint32_t sh_size;      // Section size in bytes
    uint32_t sh_link;      // Link to another section
    uint32_t sh_info;      // Additional section information
    uint32_t sh_addralign; // Section alignment
    uint32_t sh_entsize;   // Entry size if section holds table
} __packed;

struct elf64_shdr {
    uint32_t sh_name;      // Section name (string table index)
    uint32_t sh_type;      // Section type
    uint64_t sh_flags;     // Section flags
    uint64_t sh_addr;      // Section virtual address at execution
    uint64_t sh_offset;    // Section file offset
    uint64_t sh_size;      // Section size in bytes
    uint32_t sh_link;      // Link to another section
    uint32_t sh_info;      // Additional section information
    uint64_t sh_addralign; // Section alignment
    uint64_t sh_entsize;   // Entry size if section holds table
} __packed;

#define PT_NULL         UINT32_C(0)          // Program header table entry unused
#define PT_LOAD         UINT32_C(1)          // Loadable program segment
#define PT_DYNAMIC      UINT32_C(2)          // Dynamic linking information
#define PT_INTERP       UINT32_C(3)          // Program interpreter
#define PT_NOTE         UINT32_C(4)          // Auxiliary information
#define PT_SHLIB        UINT32_C(5)          // Reserved
#define PT_PHDR         UINT32_C(6)          // Entry for header table itself
#define PT_TLS          UINT32_C(7)          // Thread-local storage segment
#define PT_NUM          UINT32_C(8)          // Number of defined types
#define PT_LOOS         UINT32_C(0x60000000) // Start of OS-specific
#define PT_GNU_EH_FRAME UINT32_C(0x6474E550) // GCC .eh_frame_hdr segment
#define PT_GNU_STACK    UINT32_C(0x6474E551) // Indicates stack executability
#define PT_GNU_RELRO    UINT32_C(0x6474E552) // Read-only after relocation
#define PT_LOSUNW       UINT32_C(0x6FFFFFFA)
#define PT_SUNWBSS      UINT32_C(0x6FFFFFFA) // Sun Specific segment
#define PT_SUNWSTACK    UINT32_C(0x6FFFFFFB) // Stack segment
#define PT_HISUNW       UINT32_C(0x6FFFFFFF)
#define PT_HIOS         UINT32_C(0x6FFFFFFF) // End of OS-specific
#define PT_LOPROC       UINT32_C(0x70000000) // Start of processor-specific
#define PT_HIPROC       UINT32_C(0x7FFFFFFF) // End of processor-specific

#define PF_X        UINT32_C(0x00000001) // Segment is executable
#define PF_W        UINT32_C(0x00000002) // Segment is writable
#define PF_R        UINT32_C(0x00000004) // Segment is readable
#define PF_MASKOS   UINT32_C(0x0FF00000) // OS-specific
#define PF_MASKPROC UINT32_C(0xF0000000) // Processor-specific

struct elf32_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __packed;

struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __packed;

#define ELF32_ST_BIND(value)      (((uint8_t)(value)) >> 4)
#define ELF32_ST_TYPE(value)      ((value) & UINT8_C(0x0F))
#define ELF32_ST_INFO(bind, type) (((bind) << 4) + ((type) & UINT8_C(0x0F)))

#define ELF64_ST_BIND(value)      ELF32_ST_BIND(value)
#define ELF64_ST_TYPE(value)      ELF32_ST_TYPE(value)
#define ELF64_ST_INFO(bind, type) ELF32_ST_INFO((bind), (type))

#define STB_LOCAL      0  // Local symbol
#define STB_GLOBAL     1  // Global symbol
#define STB_WEAK       2  // Weak symbol
#define STB_NUM        3  // Number of defined types.
#define STB_LOOS       10 // Start of OS-specific
#define STB_GNU_UNIQUE 10 // Unique symbol.
#define STB_HIOS       12 // End of OS-specific
#define STB_LOPROC     13 // Start of processor-specific
#define STB_HIPROC     15 // End of processor-specific

#define STT_NOTYPE    0  // Symbol type is unspecified
#define STT_OBJECT    1  // Symbol is a data object
#define STT_FUNC      2  // Symbol is a code object
#define STT_SECTION   3  // Symbol associated with a section
#define STT_FILE      4  // Symbol's name is file name
#define STT_COMMON    5  // Symbol is a common data object
#define STT_TLS       6  // Symbol is thread-local data object
#define STT_NUM       7  // Number of defined types.
#define STT_LOOS      10 // Start of OS-specific
#define STT_GNU_IFUNC 10 // Symbol is indirect code object
#define STT_HIOS      12 // End of OS-specific
#define STT_LOPROC    13 // Start of processor-specific
#define STT_HIPROC    15 // End of processor-specific

struct elf32_sym {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
} __packed;

struct elf64_sym {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} __packed;

#define DT_NULL            0           // Marks end of dynamic section
#define DT_NEEDED          1           // Name of needed library
#define DT_PLTRELSZ        2           // Size in bytes of PLT relocs
#define DT_PLTGOT          3           // Processor defined value
#define DT_HASH            4           // Address of symbol hash table
#define DT_STRTAB          5           // Address of string table
#define DT_SYMTAB          6           // Address of symbol table
#define DT_RELA            7           // Address of Rela relocs
#define DT_RELASZ          8           // Total size of Rela relocs
#define DT_RELAENT         9           // Size of one Rela reloc
#define DT_STRSZ           10          // Size of string table
#define DT_SYMENT          11          // Size of one symbol table entry
#define DT_INIT            12          // Address of init function
#define DT_FINI            13          // Address of termination function
#define DT_SONAME          14          // Name of shared object
#define DT_RPATH           15          // Library search path (deprecated)
#define DT_SYMBOLIC        16          // Start symbol search here
#define DT_REL             17          // Address of Rel relocs
#define DT_RELSZ           18          // Total size of Rel relocs
#define DT_RELENT          19          // Size of one Rel reloc
#define DT_PLTREL          20          // Type of reloc in PLT
#define DT_DEBUG           21          // For debugging; unspecified
#define DT_TEXTREL         22          // Reloc might modify .text
#define DT_JMPREL          23          // Address of PLT relocs
#define DT_BIND_NOW        24          // Process relocations of object
#define DT_INIT_ARRAY      25          // Array with addresses of init fct
#define DT_FINI_ARRAY      26          // Array with addresses of fini fct
#define DT_INIT_ARRAYSZ    27          // Size in bytes of DT_INIT_ARRAY
#define DT_FINI_ARRAYSZ    28          // Size in bytes of DT_FINI_ARRAY
#define DT_RUNPATH         29          // Library search path
#define DT_FLAGS           30          // Flags for the object being loaded
#define DT_ENCODING        32          // Start of encoded range
#define DT_PREINIT_ARRAY   32          // Array with addresses of preinit fct
#define DT_PREINIT_ARRAYSZ 33          // size in bytes of DT_PREINIT_ARRAY
#define DT_SYMTAB_SHNDX    34          // Address of SYMTAB_SHNDX section
#define DT_NUM             35          // Number used
#define DT_LOOS            0x6000000D  // Start of OS-specific
#define DT_HIOS            0x6FFFF000  // End of OS-specific
#define DT_LOPROC          0x70000000  // Start of processor-specific
#define DT_HIPROC          0x7FFFFFFF  // End of processor-specific
#define DT_PROCNUM         DT_MIPS_NUM // Most used by any processor

struct elf32_dyn {
    uint32_t d_tag;
    union {
        uint32_t d_val;
        uint32_t d_ptr;
    } d_un;
} __packed;

struct elf64_dyn {
    uint64_t d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} __packed;

#define R_386_NONE     0
#define R_386_32       2
#define R_386_PC32     1
#define R_386_GOT32    3
#define R_386_PLT32    4
#define R_386_COPY     5
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8
#define R_386_GOTOFF   9
#define R_386_GOTPC    10
#define R_386_32PLT    11
#define R_386_16       20
#define R_386_PC16     21
#define R_386_8        22
#define R_386_PC8      23
#define R_386_SIZE32   38

#define R_X86_64_NONE      0
#define R_X86_64_64        1
#define R_X86_64_PC32      2
#define R_X86_64_GOT32     3
#define R_X86_64_PLT32     4
#define R_X86_64_COPY      5
#define R_X86_64_GLOB_DAT  6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE  8
#define R_X86_64_GOTPCREL  9
#define R_X86_64_32        10
#define R_X86_64_32S       11
#define R_X86_64_16        12
#define R_X86_64_PC16      13
#define R_X86_64_8         14
#define R_X86_64_PC8       15
#define R_X86_64_PC64      24
#define R_X86_64_GOTOFF64  25
#define R_X86_64_GOTPC32   26
#define R_X86_64_SIZE32    32
#define R_X86_64_SIZE64    33

struct elf32_rel {
    uint32_t r_offset;
    uint32_t r_info;
} __packed;

struct elf64_rel {
    uint64_t r_offset;
    uint64_t r_info;
} __packed;

struct elf32_rela {
    uint32_t r_offset;
    uint32_t r_info;
    int32_t r_addend;
} __packed;

struct elf64_rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
} __packed;

#define AT_NULL         0  // End of vector
#define AT_IGNORE       1  // Entry should be ignored
#define AT_EXECFD       2  // File descriptor of program
#define AT_PHDR         3  // Program headers for program
#define AT_PHENT        4  // Size of program header entry
#define AT_PHNUM        5  // Number of program headers
#define AT_PAGESZ       6  // System page size
#define AT_BASE         7  // Base address of interpreter
#define AT_FLAGS        8  // Flags
#define AT_ENTRY        9  // Entry point of program
#define AT_NOTELF       10 // Program is not ELF
#define AT_UID          11 // Real uid
#define AT_EUID         12 // Effective uid
#define AT_GID          13 // Real gid
#define AT_EGID         14 // Effective gid
#define AT_CLKTCK       17 // Frequency of times()
#define AT_SYSINFO      32
#define AT_SYSINFO_EHDR 33

struct elf64_auxv {
    uint64_t a_type;
    uint64_t a_val;
} __packed;

#endif /* A5B67F05_8F06_424F_9A95_ABEEBF66D8F9 */
