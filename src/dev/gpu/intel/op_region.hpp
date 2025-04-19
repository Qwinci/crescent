#pragma once

struct OpRegionHeader {
	[[nodiscard]] constexpr u16 minor_version() const {
		return over & 0xFFFFFF;
	}

	[[nodiscard]] constexpr u16 major_version() const {
		return over >> 16;
	}

	char signature[16];
	u32 size;
	u32 over;
	char sver[32];
	char vver[16];
	char gver[16];
	u32 mbox;
	u32 dmod;
	u32 pcon;
	char dver[32];
	char rm01[124];
};

struct [[gnu::packed]] OpRegionAsle {
	u32 ardy;
	u32 aslc;
	u32 tche;
	u32 alsi;
	u32 bclp;
	u32 pfit;
	u32 cblv;
	u16 bclm[20];
	u32 cpfm;
	u32 epfm;
	u8 plut[74];
	u32 pfmb;
	u32 cddv;
	u32 pcft;
	u32 srot;
	u32 iuer;
	u64 fdss;
	u32 fdsp;
	u32 stat;
	u64 rvda;
	u32 rvds;
	u8 reserved[58];
};
