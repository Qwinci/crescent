#pragma once
#include "arch/map.h"

#define X86_PF_P (1ULL << 0)
#define X86_PF_RW (1ULL << 1)
#define X86_PF_U (1ULL << 2)
#define X86_PF_WT (1ULL << 3)
#define X86_PF_CD (1ULL << 4)
#define X86_PF_A (1ULL << 5)
#define X86_PF_D (1ULL << 6)
#define X86_PF_H (1ULL << 7)
#define X86_PF_G (1ULL << 8)
#define X86_PF_PAT (1ULL << 7)
#define X86_PF_HUGE_PAT (1ULL << 12)
#define X86_PF_NX (1ULL << 63)

#define PAT0_I (0)
#define PAT1_I (X86_PF_WT)
#define PAT2_I (X86_PF_CD)
#define PAT3_I (X86_PF_CD | X86_PF_WT)
#define PAT4_I (X86_PF_PAT)
#define PAT4_HUGE_I (X86_PF_HUGE_PAT)
#define PAT5_I (X86_PF_PAT | X86_PF_WT)
#define PAT5_HUGE_I (X86_PF_HUGE_PAT | X86_PF_WT)
#define PAT6_I (X86_PF_PAT | X86_PF_CD)
#define PAT6_HUGE_I (X86_PF_HUGE_PAT | X86_PF_CD)
#define PAT7_I (X86_PF_PAT | X86_PF_CD | X86_PF_WT)
#define PAT7_HUGE_I (X86_PF_HUGE_PAT | X86_PF_CD | X86_PF_WT)

#define PAT_STRONG_UC_F PAT0_I
#define PAT_UC_F PAT1_I
#define PAT_WC_F PAT2_I
#define PAT_WT_F PAT3_I
#define PAT_WB_F PAT4_I
#define PAT_WB_HUGE_F PAT4_HUGE_I
#define PAT_WP_F PAT5_I
#define PAT_WP_HUGE_F PAT5_HUGE_I

#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000
#define PAGE_FLAG_MASK 0xFFF0000000000FFF

#define X86_HUGEPAGE_SIZE 0x200000

u64 x86_pf_from_generic(PageFlags flags);
void x86_refresh_page(usize addr);