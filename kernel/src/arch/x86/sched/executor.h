#pragma once
#include "types.h"
#include "assert.h"

typedef struct {
	u64 fs;
	u64 gs;
} ExecutorGenericState;

typedef struct {
	ExecutorGenericState generic;
	u8* simd;
} ExecutorState;

typedef struct {
	u16 fcw;
	u16 fsw;
	u8 ftw;
	u8 reserved0;
	u16 fop;
	u64 fip;
	u64 fdp;
	u32 mxcsr;
	u32 mxcsr_mask;
	u8 st0[10];
	u8 reserved1[6];
	u8 st1[10];
	u8 reserved2[6];
	u8 st2[10];
	u8 reserved3[6];
	u8 st3[10];
	u8 reserved4[6];
	u8 st4[10];
	u8 reserved5[6];
	u8 st5[10];
	u8 reserved6[6];
	u8 st6[10];
	u8 reserved7[6];
	u8 st7[10];
	u8 reserved8[6];
	u8 xmm0[16];
	u8 xmm1[16];
	u8 xmm2[16];
	u8 xmm3[16];
	u8 xmm4[16];
	u8 xmm5[16];
	u8 xmm6[16];
	u8 xmm7[16];
	u8 xmm8[16];
	u8 xmm9[16];
	u8 xmm10[16];
	u8 xmm11[16];
	u8 xmm12[16];
	u8 xmm13[16];
	u8 xmm14[16];
	u8 xmm15[16];
	u8 reserved9[48];
	u8 available[48];
} FxState;
static_assert(sizeof(FxState) == 512);
