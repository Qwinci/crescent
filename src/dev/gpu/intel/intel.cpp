#include "mem/iospace.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "utils/driver.hpp"

namespace regs {
	static constexpr BitRegister<u32> GFX_MODE_RCSUNIT {0x229C};
	static constexpr BasicRegister<u32> EXECLIST_SUBMITPORT_RCSUNIT {0x2230};
	static constexpr BitRegister<u32> EXECLIST_CONTROL_RCSUNIT {0x2550};
	static constexpr BitRegister<u32> PIPE_SRCSZ_A {0x6001C};
	static constexpr BitRegister<u32> PLANE_STRIDE_1_A {0x70188};
}

namespace gfx_mode {
	static constexpr BitField<u32, bool> GEN11_DISABLE_LEGACY_MODE {3, 1};
	static constexpr BitField<u32, bool> GEN7_EXECLIST_ENABLE {15, 1};
	static constexpr BitField<u32, u16> MASK {16, 16};
}

namespace execlist_control {
	static constexpr BitField<u32, bool> LOAD {0, 1};
}

namespace pipe_srcsz {
	static constexpr BitField<u32, u16> HEIGHT {0, 13};
	static constexpr BitField<u32, u16> WIDTH {16, 13};
}

namespace plane_stride {
	static constexpr BitField<u32, u16> STRIDE {0, 11};
}

namespace context_descriptor {
	static constexpr BitField<u64, bool> VALID {0, 1};
	static constexpr BitField<u64, bool> FORCE_RESTORE {2, 1};
	static constexpr BitField<u64, u8> FAULT_HANDLING {6, 2};
	// indicates PPGTT is enabled in legacy context mode (must be zero in advanced context mode)
	static constexpr BitField<u64, bool> PRIVILEGE_ACCESS {8, 1};
	// 4kb aligned logical ring context address in GGTT
	static constexpr BitField<u64, u32> LRCA {12, 20};
	static constexpr BitField<u64, u8> CTX_ID_VIRT_FUNC_NUM {32, 5};
	// unique software assigned context id (max 2048)
	static constexpr BitField<u64, u8> CTX_ID_SW_CTX_ID {37, 11};
	static constexpr BitField<u64, u8> CTX_ID_ENGINE_INSTANCE {48, 6};
	static constexpr BitField<u64, u8> CTX_ID_SW_COUNTER {55, 6};
	static constexpr BitField<u64, u8> CTX_ID_ENGINE_CLASS {61, 3};

	static constexpr u8 FAULT_AND_HANG = 0;
}

namespace engine_instance {
	// render engine
	static constexpr u8 RCS = 0;
	// copy engine
	static constexpr u8 BCS = 0;
}

namespace engine_class {
	static constexpr u8 RENDER = 0;
	static constexpr u8 COPY = 3;
}

struct Generation {
	u8 gen;
};

static constexpr Generation GEN7 {7};
static constexpr Generation GEN11 {11};

// assume this is enough for all gens that this driver supports (should be true for gens <= 12)
// this also includes the hardware status page.
static constexpr usize MAX_LOGICAL_CTX_SIZE = 22 * 0x1000;

namespace mi_cmds {
	static constexpr u8 BATCH_BUFFER_END = 0xA;
	static constexpr u8 LOAD_REGISTER_IMM = 0x22;
}

namespace mi_flags {
	// for load_register_imm
	static constexpr u32 ADD_CS_MMIO_START_OFFSET = 1 << 19;
	static constexpr u32 FORCE_POSTED = 1 << 12;

	// for batch_buffer_end
	static constexpr u32 END_CONTEXT = 1 << 0;
}

static constexpr u32 mi_cmd(u8 cmd, u32 flags) {
	return (cmd << 23) | flags;
}

static constexpr u32 mi_load_register_imm(u8 count, bool posted = true) {
	return mi_cmd(
		mi_cmds::LOAD_REGISTER_IMM,
		(count * 2 - 1) |
		mi_flags::ADD_CS_MMIO_START_OFFSET |
		(posted ? mi_flags::FORCE_POSTED : 0));
}

static constexpr u32 mi_batch_buffer_end(bool end_ctx) {
	return mi_cmd(mi_cmds::BATCH_BUFFER_END, end_ctx ? mi_flags::END_CONTEXT : 0);
}

namespace {
	constexpr usize CTX_RING_BUFFER_HEAD = 5;
	constexpr usize CTX_RING_TAIL_POINTER = 7;
	constexpr usize CTX_RING_BUFFER_START = 9;
	constexpr usize CTX_RING_BUFFER_CONTROL = 11;
}

void gen11_init_logical_offsets(u32* regs) {
	// NOOP
	regs[0] = 0;
	regs[1] = mi_load_register_imm(13);
	// context control
	regs[2] = 0x244;
	// Ring Buffer Head
	regs[4] = 0x34;
	// Ring Tail Pointer Register
	regs[6] = 0x30;
	// RING_BUFFER_START
	regs[8] = 0x38;
	// RING_BUFFER_CONTROL
	regs[10] = 0x3C;
	// Batch Buffer Current Head Register (UDW = Upper Dword)
	regs[12] = 0x168;
	// Batch Buffer Current Head Register
	regs[14] = 0x140;
	// Batch Buffer State Register
	regs[16] = 0x110;
	// BB_PER_CTX_PTR
	regs[18] = 0x1C0;
	// RCS_INDIRECT_CTX
	regs[20] = 0x1C4;
	// RCS_INDIRECT_CTX_OFFSET
	regs[22] = 0x1C8;
	// CCID
	regs[24] = 0x180;
	// SEMAPHORE_TOKEN
	regs[26] = 0x2B4;
	// NOOP 5x
	regs[28] = 0;
	regs[29] = 0;
	regs[30] = 0;
	regs[31] = 0;
	regs[32] = 0;
	regs[33] = mi_load_register_imm(9);
	// CTX_TIMESTAMP
	regs[34] = 0x3A8;
	// PDP3_UDW
	regs[36] = 0x28C;
	// PDP3_LDW
	regs[38] = 0x288;
	// PDP2_UDW
	regs[40] = 0x284;
	// PDP2_LDW
	regs[42] = 0x280;
	// PDP1_UDW
	regs[44] = 0x27C;
	// PDP1_LDW
	regs[46] = 0x278;
	// PDP0_UDW
	regs[48] = 0x274;
	// PDP0_LDW
	regs[50] = 0x270;
	regs[52] = mi_load_register_imm(3);
	// POSH_LRCA
	regs[53] = 0x1B0;
	// CONTEXT_SCHEDULING_ATTRIBUTES (RESOURCE_MIN_MAX_PRIRITY)
	regs[55] = 0x5A8;
	// PREEMPTION_STATUS (RO)
	regs[57] = 0x5AC;
	// NOOP x6
	regs[59] = 0;
	regs[60] = 0;
	regs[61] = 0;
	regs[62] = 0;
	regs[63] = 0;
	regs[64] = 0;
	regs[65] = mi_load_register_imm(1, false);
	// R_PWR_CLK_STATE
	regs[66] = 0xC8;
	// NOOP x13
	regs[68] = 0;
	regs[69] = 0;
	regs[70] = 0;
	regs[71] = 0;
	regs[72] = 0;
	regs[73] = 0;
	regs[74] = 0;
	regs[75] = 0;
	regs[76] = 0;
	regs[77] = 0;
	regs[78] = 0;
	regs[79] = 0;
	regs[80] = 0;
	regs[81] = mi_load_register_imm(51);
	// BB_STACK_WRITE_PORT x6
	regs[82] = 0x588;
	regs[84] = 0x588;
	regs[86] = 0x588;
	regs[88] = 0x588;
	regs[90] = 0x588;
	regs[92] = 0x588;
	// EXCC
	regs[94] = 0x28;
	// MI_MODE
	regs[96] = 0x9C;
	// INSTPM
	regs[98] = 0xC0;
	// PR_CTR_CTL
	regs[100] = 0x178;
	// PR_CTR_THRSH
	regs[102] = 0x17C;
	// TIMESTAMP Register (LSB)
	regs[104] = 0x358;
	// BB_START_ADDR_UDW
	regs[106] = 0x170;
	// BB_START_ADDR
	regs[108] = 0x150;
	// BB_ADD_DIFF
	regs[110] = 0x154;
	// BB_OFFSET
	regs[112] = 0x158;
	// MI_PREDICATE_RESULT_1
	regs[114] = 0x41C;
	// CS_GPR (1-16)
	regs[116] = 0x600;
	regs[118] = 0x604;
	regs[120] = 0x608;
	regs[122] = 0x60C;
	regs[124] = 0x610;
	regs[126] = 0x614;
	regs[128] = 0x618;
	regs[130] = 0x61C;
	regs[132] = 0x620;
	regs[134] = 0x624;
	regs[136] = 0x628;
	regs[138] = 0x62C;
	regs[140] = 0x630;
	regs[142] = 0x634;
	regs[144] = 0x638;
	regs[146] = 0x63C;
	regs[148] = 0x640;
	regs[150] = 0x644;
	regs[152] = 0x648;
	regs[154] = 0x64C;
	regs[156] = 0x650;
	regs[158] = 0x654;
	regs[160] = 0x658;
	regs[162] = 0x65C;
	regs[164] = 0x660;
	regs[166] = 0x664;
	regs[168] = 0x668;
	regs[170] = 0x66C;
	regs[172] = 0x670;
	regs[174] = 0x674;
	regs[176] = 0x678;
	regs[178] = 0x67C;
	// IPEHR
	regs[180] = 0x68;
	// CMD_BUF_CCTL
	regs[182] = 0x84;
	// NOOP
	regs[184] = 0;
	regs[185] = mi_batch_buffer_end(true);
}

namespace ctx_control {
	// prevent restoring engine context
	static constexpr BitField<u32, bool> ENGINE_CTX_RESTORE_INHIBIT {0, 1};
	// (?)
	static constexpr BitField<u32, bool> RS_CTX_ENABLE {1, 1};
	// inhibit synchronous context switch (?)
	static constexpr BitField<u32, bool> INHIBIT_SYN_CTX_SWITCH {3, 1};
	static constexpr BitField<u32, bool> GEN11_RUNALONE_MODE {7, 1};
	static constexpr BitField<u32, bool> GEN11_OAR_CONTEXT_ENABLE {8, 1};
	static constexpr BitField<u32, u16> MASK {16, 16};
}

namespace ring_buffer_ctl {
	static constexpr BitField<u32, bool> ENABLE {0, 1};
	static constexpr BitField<u32, u16> BUFFER_LENGTH {12, 9};
}

namespace mi_mode {
	static constexpr BitField<u32, bool> STOP_RINGS {8, 1};
	static constexpr BitField<u32, u16> MASK {16, 16};
}

struct LogicalContext {
	void init() {
		// this isn't really necessary, the gtt can be discontiguous
		pages = pmalloc(ALIGNUP(MAX_LOGICAL_CTX_SIZE, PAGE_SIZE) / PAGE_SIZE);
		assert(pages);
		virt = KERNEL_VSPACE.alloc(MAX_LOGICAL_CTX_SIZE);
		assert(virt);
		for (usize i = 0; i < MAX_LOGICAL_CTX_SIZE; i += PAGE_SIZE) {
			auto status = KERNEL_PROCESS->page_map.map(
				reinterpret_cast<u64>(virt) + i,
				pages + i,
				PageFlags::Read | PageFlags::Write,
				CacheMode::Uncached);
			assert(status);
		}

		memset(virt, 0, MAX_LOGICAL_CTX_SIZE);

		regs = offset(virt, u32*, 0x1000);
		gen11_init_logical_offsets(regs);

		u32 ctx_control =
			ctx_control::INHIBIT_SYN_CTX_SWITCH(true) |
			ctx_control::ENGINE_CTX_RESTORE_INHIBIT(true) |
			ctx_control::MASK(
				(1U << ctx_control::INHIBIT_SYN_CTX_SWITCH.offset) |
				(1U << ctx_control::ENGINE_CTX_RESTORE_INHIBIT.offset));
		regs[3] = ctx_control;
		// timestamp
		regs[35] = 0;
		// BB_OFFSET
		regs[113] = 0;

		// todo set ppgtt

		// MI_MODE
		regs[97] = mi_mode::STOP_RINGS(false) | mi_mode::MASK(1U << mi_mode::STOP_RINGS.offset);
	}

	void* virt;
	u32* regs;
	usize pages;
};

static constexpr u64 GTT_PRESENT = 1 << 0;

struct CmdRing {
	CmdRing() {
		page = pmalloc(1);
		assert(page);

		start = static_cast<u32*>(KERNEL_VSPACE.alloc(0x1000));
		assert(start);
		auto status = KERNEL_PROCESS->page_map.map(
			reinterpret_cast<u64>(start),
			page,
			PageFlags::Read | PageFlags::Write,
			CacheMode::Uncached);
		assert(status);
		end = start + (0x1000 / 4);
		ptr = start;
		count = 0;

		clear();
	}

	void clear() {
		memset(start, 0, 0x1000);
	}

	void reset() {
		ptr = start;
		count = 0;
	}

	void add_dw(u32 dw) {
		assert(count != PAGE_SIZE / 4);

		*ptr++ = dw;
		++count;
		if (ptr == end) {
			ptr = start;
		}
	}

	[[nodiscard]] u32 finish_get_tail() {
		usize dws = ptr - start;
		if (dws % 2) {
			*ptr++ = 0;
		}
		++dws;
		return dws * 4;
	}

	u32* start;
	u32* end;
	u32* ptr;
	usize page;
	usize count;
};

namespace cmd_type {
	static constexpr u32 GFX_PIPE = 3 << 29;
}

namespace cmd_subtype {
	static constexpr u32 GFX_PIPE_COMMON = 0 << 27;
	static constexpr u32 GFX_PIPE_3D = 3 << 27;
}

namespace _3d_cmd {
	static constexpr u32 _3DSTATE_PIPELINED = 0;
	static constexpr u32 GFXPIPE_NONPIPELINED = 1 << 24;
	static constexpr u32 _3DPRIMITIVE = 3 << 24;
}

namespace _3d_subcmd {
	static constexpr u32 STATE_BASE_ADDRESS = 0x1 << 16;
	static constexpr u32 _3DSTATE_VERTEX_BUFFERS = 0x8 << 16;
	static constexpr u32 _3DSTATE_VERTEX_ELEMENTS = 0x9 << 16;
	static constexpr u32 _3DSTATE_VF = 0xC << 16;
	static constexpr u32 _3DSTATE_CC_STATE_POINTERS = 0xE << 16;
	static constexpr u32 _3DSTATE_SCISSOR_STATE_POINTERS = 0xF << 16;
	static constexpr u32 _3DSTATE_VS = 0x10 << 16;
	static constexpr u32 _3DSTATE_CLIP = 0x12 << 16;
	static constexpr u32 _3DSTATE_SF = 0x13 << 16;
	static constexpr u32 _3DSTATE_WM = 0x14 << 16;
	static constexpr u32 _3DSTATE_STREAMOUT = 0x1E << 16;
	static constexpr u32 _3DSTATE_SBE = 0x1F << 16;
	static constexpr u32 _3DSTATE_PS = 0x20 << 16;
	static constexpr u32 _3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP = 0x21 << 16;
	static constexpr u32 _3DSTATE_VIEWPORT_STATE_POINTERS_CC = 0x23 << 16;
	static constexpr u32 _3DSTATE_BLEND_STATE_POINTERS = 0x24 << 16;
	static constexpr u32 _3DSTATE_VF_INSTANCING = 0x49 << 16;
	static constexpr u32 _3DSTATE_RASTER = 0x50 << 16;
	static constexpr u32 _3DSTATE_VF_TOPOLOGY = 0x4B << 16;
}

enum class ExecSize : u64 {
	_1 = 0,
	_2 = 1,
	_4 = 0b10,
	_8 = 0b11,
	_16 = 0b100,
	_32 = 0b101
};

struct ShaderBuilder {
	struct Message {
		enum class DataFormat {
			_16bit = 1,
			_32bit = 0
		};

		bool end_of_thread;
		u8 response_reg_count;
		u8 msg_reg_count;
		// from src1
		u8 extended_reg_count;

		bool dest_is_gp_reg;
		u8 dest_reg_num;
		// selects the byte within the register for GRF
		u8 dest_reg_offset;

		bool src0_is_gp_reg;
		u8 src0_reg_num;

		bool src1_is_gp_reg;
		u8 src1_reg_num;

		u32 func_control;
		u32 extended_func_control;
		bool header_present;
		// width of data read from sampler or written to render target
		DataFormat fmt;
		bool cps_lod_compensation;
		u8 shared_func_id;
	};

	void send(Message msg) {
		assert(msg.response_reg_count <= 16);
		assert(msg.extended_reg_count <= 31);
		assert(msg.msg_reg_count != 0);
		assert(msg.msg_reg_count <= 15);
		assert(msg.dest_reg_offset <= 31);
		assert((msg.func_control & ((1 << 19) - 1)) == 0);
		assert((msg.extended_func_control & ~((1 << 20) - 1)) == 0);
		assert(msg.shared_func_id <= 0b1111);

		u64 first = 0;
		// op
		first |= 0x31;
		first |= static_cast<u64>(exec_size) << 16;
		first |= static_cast<u64>(msg.end_of_thread) << 34;
		// ExDesc[11]
		first |= static_cast<u64>(msg.cps_lod_compensation) << 35;
		// ExDesc[23:11]
		first |= static_cast<u64>(msg.extended_func_control & 0b111111111111) << 36;
		// dst GRF (0 == ARF)
		first |= static_cast<u64>(msg.dest_is_gp_reg) << 50;
		// response length in GRFs (0-16)
		first |= static_cast<u64>(msg.response_reg_count) << 51;
		// dst reg num
		first |= static_cast<u64>(msg.dest_reg_num) << 56;

		u8 simd_mode = 0b000;

		u64 second = 0;
		// ExDesc[25:24]
		second |= msg.extended_func_control >> 12 & 0b11;
		// src0 GRF (0 == ARF)
		second |= static_cast<u64>(msg.src0_is_gp_reg) << 2;
		// message length in GRFs (1-15)
		second |= static_cast<u64>(msg.msg_reg_count) << 3;
		// simd mode upper bit
		second |= static_cast<u64>(simd_mode >> 2) << 7;
		// src0 reg num
		second |= static_cast<u64>(msg.src0_reg_num) << 8;
		// MsgDesc[10:0]
		second |= static_cast<u64>(msg.func_control & 0b11111111111) << 17;
		// SFID
		second |= static_cast<u64>(msg.shared_func_id) << 28;
		// ExDesc[27:26]
		second |= static_cast<u64>(msg.extended_func_control >> 14 & 0b11) << 32;
		// src1 GRF (0 == ARF)
		second |= static_cast<u64>(msg.src1_is_gp_reg) << 34;
		// extended message length in GRFs (0-31)
		second |= static_cast<u64>(msg.extended_reg_count) << 35;
		// src1 reg num
		second |= static_cast<u64>(msg.src1_reg_num) << 40;
		// MsgDesc[18:11]
		second |= static_cast<u64>(msg.func_control >> 11) << 49;
		// MsgDesc[19]
		second |= static_cast<u64>(msg.header_present) << 57;
		// MsgDesc[31:30]
		second |= static_cast<u64>(msg.fmt) << 58;
		// ExDesc[31:28]
		second |= static_cast<u64>(msg.extended_func_control >> 16) << 60;

		data.push(first);
		data.push(second);
	}

	kstd::vector<u64> data {};
	ExecSize exec_size {};
};

namespace sfid {
	static constexpr u8 NIL = 0;
	static constexpr u8 SAMPLER = 0b10;
	static constexpr u8 MSG_GATEWAY = 0b11;
	static constexpr u8 DATA_CACHE_DATA_PORT2 = 0b100;
	static constexpr u8 RENDER_CACHE_DATA_PORT = 0b101;
	static constexpr u8 URB = 0b110;
	static constexpr u8 THREAD_SPAWNER = 0b111;
	static constexpr u8 VIDEO_MOTION_ESTIMATION = 0b1000;
	static constexpr u8 DATA_CACHE_READ_ONLY_DATA_PORT = 0b1001;
	static constexpr u8 DATA_CACHE_DATA_PORT = 0b1010;
	static constexpr u8 PIXEL_INTERPOLATOR = 0b1011;
	static constexpr u8 DATA_CACHE_DATA_PORT1 = 0b1100;
	static constexpr u8 CHECK_AND_REFINEMENT_ENGINE = 0b1101;
}

struct IntelGpu {
	explicit IntelGpu(pci::Device& device) {
		auto bar_size = device.get_bar_size(0);

		space = IoSpace {device.map_bar(0)};
		gtt_space = space.subspace(bar_size / 2);
		gtt_size = bar_size / 2;

		auto height = (space.load(regs::PIPE_SRCSZ_A) & pipe_srcsz::HEIGHT) + 1;
		auto stride = (space.load(regs::PLANE_STRIDE_1_A) & plane_stride::STRIDE) * 64;

		usize initial_fb_gtt_entries = ALIGNUP(height * stride, PAGE_SIZE) / PAGE_SIZE;

		// clear the ggtt leaving the initial framebuffer
		for (usize i = initial_fb_gtt_entries * 8; i < gtt_size; i += 8) {
			gtt_space.store<u64>(i, 0);
		}

		auto gen = static_cast<const Generation*>(device.driver_data)->gen;

		BitValue<u32> gfx_mode {};
		if (gen >= 11) {
			gfx_mode |= gfx_mode::GEN11_DISABLE_LEGACY_MODE(true);
			gfx_mode |= gfx_mode::MASK(1U << gfx_mode::GEN11_DISABLE_LEGACY_MODE.offset);
		}
		else {
			gfx_mode |= gfx_mode::GEN7_EXECLIST_ENABLE(true);
			gfx_mode |= gfx_mode::MASK(1U << gfx_mode::GEN7_EXECLIST_ENABLE.offset);
		}
		space.store(regs::GFX_MODE_RCSUNIT, gfx_mode);

		LogicalContext ctx {};
		ctx.init();

		// allocate logical context in ggtt
		usize logical_ctx_gtt_start = ggtt_alloc(ALIGNUP(MAX_LOGICAL_CTX_SIZE, 0x1000) / 0x1000);
		assert(logical_ctx_gtt_start);
		for (usize i = 0; i < MAX_LOGICAL_CTX_SIZE; i += 0x1000) {
			ggtt_map(logical_ctx_gtt_start + i, ctx.pages + i);
		}

		usize vertex_buffer_page = pmalloc(1);
		assert(vertex_buffer_page);

		static constexpr float vertices[] {
			-0.5f, -0.5f, 0.0f,
			0.5f, -0.5f, 0.0f,
			0.0f,  0.5f, 0.0f
		};
		memcpy(to_virt<void>(vertex_buffer_page), vertices, sizeof(vertices));

		usize gtt_vertex_buffer_start = ggtt_alloc(1);
		assert(gtt_vertex_buffer_start);
		ggtt_map(gtt_vertex_buffer_start, vertex_buffer_page);
		usize vertex_buffer_size = sizeof(vertices);

		// general state contains scratch space (configured to 1kb for vertex and 1kb for pixel)
		usize general_state_page = pmalloc(1);
		assert(general_state_page);
		memset(to_virt<void>(general_state_page), 0, PAGE_SIZE);
		usize general_state_gtt = ggtt_alloc(1);
		assert(general_state_gtt);
		ggtt_map(general_state_gtt, general_state_page);

		// surface state contains SURFACE_STATE
		usize surface_state_page = pmalloc(1);
		assert(surface_state_page);
		memset(to_virt<void>(surface_state_page), 0, PAGE_SIZE);
		usize surface_state_gtt = ggtt_alloc(1);
		assert(surface_state_gtt);
		ggtt_map(surface_state_gtt, surface_state_page);

		auto* surface_ptr = to_virt<u32>(surface_state_page);
		surface_ptr[0] =
			// surface type 2d
			1 << 29 |
			// surface format
			// B8G8R8X8_UNORM = 0xE9
			// B8G8R8X8_UNORM_SRGB = 0xEA
			// R8G8B8X8_UNORM = 0xEB
			// R8G8B8X8_UNORM_SRGB = 0xEC
			0xE9 << 18 |
			// surface vertical alignment 4 j
			1 << 16 |
			// surface horizontal alignment 16 j
			3 << 14;
		surface_ptr[1] =
			// unorm path enable in color pipe
			1 << 31 |
			// sample tap discard enable (must be set for non alpha formats)
			1 << 15 |
			// surface qpitch
			// distance in rows between array slices (including mipmaps)
			// todo minus 1?
			(1080 / 4);
		surface_ptr[2] =
			// height - 1
			(1080 - 1) << 16 |
			// width - 1
			(1920 - 1);
		surface_ptr[3] =
			// number of array levels - 1
			0 << 21 |
			// surface pitch
			(1080 * 4);
		surface_ptr[4] = 0;
		surface_ptr[5] = 0;
		surface_ptr[6] = 0;
		surface_ptr[7] =
			// shader channel select red = SL_RED
			4 << 25 |
			// shader channel select green = SL_GREEN
			5 << 22 |
			// shader channel select blue = SL_BLUE
			6 << 19;
		// surface base address (64-bit)
		surface_ptr[8] = 0;
		surface_ptr[9] = 0;
		// aux surface base address (4kb aligned)
		surface_ptr[10] = 0;
		surface_ptr[11] = 0;
		surface_ptr[12] = 0;
		surface_ptr[13] = 0;
		surface_ptr[14] = 0;
		surface_ptr[15] = 0;

		usize dynamic_state_page = pmalloc(1);
		assert(dynamic_state_page);
		memset(to_virt<void>(dynamic_state_page), 0, PAGE_SIZE);
		usize dynamic_state_gtt = ggtt_alloc(1);
		assert(dynamic_state_gtt);
		ggtt_map(dynamic_state_gtt, dynamic_state_page);

		usize instruction_page = pmalloc(1);
		assert(instruction_page);
		memset(to_virt<void>(instruction_page), 0, PAGE_SIZE);
		usize instruction_gtt = ggtt_alloc(1);
		assert(instruction_gtt);
		ggtt_map(instruction_gtt, instruction_page);

		// 32-byte aligned offset of CC_VIEWPORT * 16 state from Dynamic State Base Address
		u32 viewport_gtt_offset = 0;
		// 64-byte aligned offset of SF_CLIP_VIEWPORT * 16 state from Dynamic State Base Address
		u32 clip_viewport_gtt_offset = 16 * 8;
		// 32-byte aligned offset of SCISSOR_RECT * 16 state from Dynamic State Base Address
		u32 scissor_gtt_offset = clip_viewport_gtt_offset + 16 * 64;

		// 64-byte aligned offset of COLOR_CALC_STATE from Dynamic State Base Address
		u32 color_calc_state_offset = scissor_gtt_offset + 16 * 8;

		// 64-byte aligned offset of BLEND_STATE * 8 from Dynamic State Base Address
		u32 blend_state_offset = color_calc_state_offset + 16 * 4;

		// verify that the clip viewport state is aligned (comes after viewport state)
		static_assert(16 * 8 % 64 == 0);
		// verify that the scissor state is aligned (comes after clip viewport state)
		static_assert((16 * 8 + 16 * 64) % 32 == 0);
		// verify that the color calc state is aligned (comes after scissor state)
		static_assert((16 * 8 + 16 * 64 + 16 * 8) % 64 == 0);
		// verify that the blend state is aligned (comes aligned after color calc state)
		static_assert((16 * 8 + 16 * 64 + 16 * 8 + 16 * 4) % 64 == 0);

		auto viewport_ptr = to_virt<u32>(dynamic_state_page);
		// 16x of the following:
		// first dword is min depth (ieee float)
		// second dword is max depth (ieee float)

		auto clip_viewport_ptr = to_virt<u32>(dynamic_state_page + clip_viewport_gtt_offset);
		// 16x of 64-byte structures specifying clip viewport extents (clipping is disabled so not needed)

		auto scissor_ptr = to_virt<u32>(dynamic_state_page + scissor_gtt_offset);
		// 16x of the following:
		// first dword bits 31:16 is scissor y min (u16)
		// first dword bits 15:0 is scissor x min (u16)
		// second dword bits 31:16 is scissor y max (u16)
		// second dword bits 15:0 is scissor x max (u16)

		auto color_ptr = to_virt<u32>(dynamic_state_page + color_calc_state_offset);
		color_ptr[0] = 0;
		color_ptr[1] = 0;
		color_ptr[2] = 0;
		color_ptr[3] = 0;
		color_ptr[4] = 0;
		color_ptr[5] = 0;

		auto blend_ptr = to_virt<u32>(dynamic_state_page + blend_state_offset);
		blend_ptr[0] = 0;
		// blend state entry 0 (disable blending)
		blend_ptr[0] = 0;
		blend_ptr[1] = 0;

		ShaderBuilder vert_builder {};
		vert_builder.send({
			.end_of_thread = true,
			.response_reg_count = 0,
			.msg_reg_count = 0,
			.extended_reg_count = 0,
			.dest_is_gp_reg = true,
			.dest_reg_num = 0,
			.dest_reg_offset = 0,
			.src0_is_gp_reg = true,
			.src0_reg_num = 0,
			.src1_is_gp_reg = true,
			.src1_reg_num = 0,
			.func_control = 0,
			.extended_func_control = 0,
			.header_present = false,
			.fmt = ShaderBuilder::Message::DataFormat::_16bit,
			.cps_lod_compensation = false,
			.shared_func_id = sfid::URB
		});

		ShaderBuilder pixel_builder {};
		pixel_builder.send({
			.end_of_thread = true,
			.response_reg_count = 0,
			.msg_reg_count = 0,
			.extended_reg_count = 0,
			.dest_is_gp_reg = true,
			.dest_reg_num = 0,
			.dest_reg_offset = 0,
			.src0_is_gp_reg = true,
			.src0_reg_num = 0,
			.src1_is_gp_reg = true,
			.src1_reg_num = 0,
			.func_control = 0,
			.extended_func_control = 0,
			.header_present = false,
			.fmt = ShaderBuilder::Message::DataFormat::_16bit,
			.cps_lod_compensation = false,
			.shared_func_id = sfid::RENDER_CACHE_DATA_PORT
		});

		assert(vert_builder.data.size() * 8 % 64 == 0);
		assert(pixel_builder.data.size() * 8 % 64 == 0);

		auto inst_ptr = to_virt<u64>(instruction_page);
		memcpy(inst_ptr, vert_builder.data.data(), vert_builder.data.size() * 8);
		memcpy(inst_ptr + vert_builder.data.size(), pixel_builder.data.data(), pixel_builder.data.size() * 8);

		// 64-byte aligned vertex shader Kernel Start Pointer (offset from Instruction Base Address)
		usize vertex_shader_gtt_offset = 0;
		// 64-byte aligned pixel shader Kernel Start Pointer 0 (offset from Instruction Base Address)
		usize pixel_shader_gtt_offset = vert_builder.data.size() * 8;

		CmdRing ring {};

		// setup vertex fetch topology
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_VF_TOPOLOGY |
			// length in dw ignoring dw - 2
			0);
		// 3D_Prim_Topo_Type 3DPRIM_TRILIST
		ring.add_dw(4);

		// setup vertex fetch state using 3DSTATE_VF (might not be necessary)
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_VF |
			// length in dw ignoring dw - 2
			0);
		ring.add_dw(0);

		// setup vertex buffer
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_VERTEX_BUFFERS |
			// length in dw ignoring dw - 2
			3);
		ring.add_dw(
			// Buffer Pitch (stride)
			3 * sizeof(float) |
			// Address Modify Enable (Buffer Starting Address is used)
			1 << 14 |
			// 16:24 == MEMORY_OBJECT_CONTROL_STATE
			// vertex buffer index
			0 << 26);
		// Buffer Starting Address
		ring.add_dw(gtt_vertex_buffer_start);
		ring.add_dw(gtt_vertex_buffer_start >> 32);
		// Buffer Size
		ring.add_dw(vertex_buffer_size);

		// setup vertex attributes
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_VERTEX_ELEMENTS |
			// length in dw ignoring dw - 2
			1);
		ring.add_dw(
			// source element offset in bytes in the vertex structure
			0 |
			// 24:16 source element format (SURFACE_FORMAT)
			// R32G32B32_FLOAT
			0x40 << 16 |
			// valid
			1 << 25 |
			// vertex buffer index
			0 << 26);
		// VFCOMP_NOSTORE = 0
		// VFCOMP_STORE_SRC = 1
		// VFCOMP_STORE_0 = 2
		// VFCOMP_STORE_1_FP = 3
		// VFCOMP_STORE_1_INT = 4
		ring.add_dw(
			// 30:28 component 0 control (3D_Vertex_Component_Control)
			1 << 28 |
			// 26:24 component 1 control (3D_Vertex_Component_Control)
			1 << 24 |
			// 22:20 component 2 control (3D_Vertex_Component_Control)
			1 << 20 |
			// 18:16 component 3 control (3D_Vertex_Component_Control)
			0 << 16);

		// disable instancing for attribute 0
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_VF_INSTANCING |
			// length in dw ignoring dw - 2 (0x43 == context restore)
			1);
		ring.add_dw(
			// vertex element index (0-33)
			0 |
			// instancing enable
			0 << 8);
		// instance advancement rate (if instancing enabled, determines after how many instances new vertex data is provided)
		ring.add_dw(0);

		// setup vertex shader
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_VS |
			// length in dw ignoring dw - 2
			7);
		// Kernel Start Pointer (64-byte aligned offset from Instruction Base Address)
		ring.add_dw(vertex_shader_gtt_offset);
		ring.add_dw(vertex_shader_gtt_offset >> 32);
		// misc flags
		ring.add_dw(0);
		ring.add_dw(
			// Scratch Space Base Pointer (1kb aligned) from General State Base Address
			0 |
			// Per-Thread Scratch Space (0 == 1kb, 11 == 2mb)
			0);
		// reserved
		ring.add_dw(0);
		ring.add_dw(
			// 9:4 == Vertex URB Entry Read Offset
			0 << 4 |
			// 11:16 == Vertex URB Entry Read Length (Number of pairs of 128-bit vertex elements to be passed into the payload for each vertex), max 15
			1 << 11 |
			// 24:20 == dispatch GRF start register offset for URB (Universal Return Buffer) data in thread payload
			// R0 == 0, R31 == 31
			0 << 20);
		ring.add_dw(
			// bit 0 == function enable
			1 << 0 |
			// bit 1 == vertex cache disable
			// SIMD8 Dispatch Enable (must be enabled)
			1 << 2 |
			// 31:22 == Maximum number of threads - 1 (used scratch space is affected)
			// max == 546
			1 << 22);
		ring.add_dw(
			// 16:20 == Vertex URB Entry Output Length in 256-bit units
			1 << 16 |
			// 26:21 == Vertex URB Entry Output Read Offset (offset in 256-bit units of data of start of attribute 0 to be passed to pixel shader)
			// should skip vertex header (not done here)
			0 << 21);

		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_STREAMOUT |
			// length in dw ignoring dw - 2
			3);
		ring.add_dw(0);
		ring.add_dw(0);
		ring.add_dw(0);
		ring.add_dw(0);

		// setup state address
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_COMMON |
			_3d_cmd::GFXPIPE_NONPIPELINED |
			_3d_subcmd::STATE_BASE_ADDRESS |
			// length in dw ignoring dw - 2
			20);
		// General State Base Address (4kb aligned)
		ring.add_dw(
			// low 32 bits of address
			general_state_gtt |
			// General State Base Address Modify Enable
			1);
		// high 32 bits of address
		ring.add_dw(general_state_gtt >> 32);
		ring.add_dw(0);
		// Surface State Base Address (4kb aligned)
		ring.add_dw(
			// low 32 bits of address
			surface_state_gtt |
			// Surface State Base Address Modify Enable
			1);
		// high 32 bits of address
		ring.add_dw(surface_state_gtt >> 32);
		// Dynamic State Base Address (4kb aligned)
		ring.add_dw(
			// low 32 bits of address
			dynamic_state_gtt |
			// Dynamic State Base Address Modify Enable
			1);
		// high 32 bits of address
		ring.add_dw(dynamic_state_gtt >> 32);
		// Indirect Object Base Address (4kb aligned)
		ring.add_dw(
			// low 32 bits of address
			0 |
			// Indirect Object Base Address Modify Enable
			1);
		// high 32 bits of address
		ring.add_dw(0);
		// Instruction Base Address (4kb aligned)
		ring.add_dw(
			// low 32 bits of address
			instruction_gtt |
			// Instruction Base Address Modify Enable
			1);
		// high 32 bits of address
		ring.add_dw(instruction_gtt >> 32);
		ring.add_dw(
			// 31:12 General State Buffer Size (in 4kb pages)
			1 << 12 |
			// General State Buffer Size Modify Enable
			1);
		ring.add_dw(
			// 31:12 Dynamic State Buffer Size (in 4kb pages)
			1 << 12 |
			// Dynamic State Buffer Size Modify Enable
			1);
		ring.add_dw(
			// 31:12 Indirect Object Buffer Size (in 4kb pages)
			// Indirect Object Buffer Size Modify Enable
			1);
		ring.add_dw(
			// 31:12 Instruction Buffer Size (in 4kb pages)
			1 << 12 |
			// Instruction Buffer Size Modify Enable
			1);
		// Bindless Surface State Base Address (4kb aligned)
		ring.add_dw(
			// low 32 bits of address
			0 |
			// Bindless Surface State Base Address Modify Enable
			1);
		// high 32 bits of address
		ring.add_dw(0);
		ring.add_dw(
			// 31:12 Bindless Surface State Size (in 64-byte increments - 1)
			0);
		// Bindless Sampler State Base Address (4kb aligned)
		ring.add_dw(
			// low 32 bits of address
			0 |
			// Bindless Sampler State Base Address Modify Enable
			1);
		// high 32 bits of address
		ring.add_dw(0);
		ring.add_dw(
			// 31:12 Bindless Sampler State Buffer Size (in 4kb pages)
			0);

		// setup viewport pointers
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_VIEWPORT_STATE_POINTERS_CC |
			// length in dw ignoring dw - 2
			0);
		// 32-byte aligned offset of CC_VIEWPORT * 16 state from Dynamic State Base Address
		ring.add_dw(viewport_gtt_offset);

		// setup clip viewport pointers
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP |
			// length in dw ignoring dw - 2
			0);
		// 64-byte aligned offset of SF_CLIP_VIEWPORT * 16 state from Dynamic State Base Address
		ring.add_dw(clip_viewport_gtt_offset);

		// setup scissor pointers
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_SCISSOR_STATE_POINTERS |
			// length in dw ignoring dw - 2
			0);
		// 32-byte aligned offset of SCISSOR_RECT * 16 state from Dynamic State Base Address
		ring.add_dw(scissor_gtt_offset);

		// setup color state pointers
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_CC_STATE_POINTERS |
			// length in dw ignoring dw - 2
			0);
		// 64-byte aligned offset of COLOR_CALC_STATE from Dynamic State Base Address
		ring.add_dw(
			color_calc_state_offset |
			// color calc state pointer valid
			1);

		// setup blend state pointers
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_BLEND_STATE_POINTERS |
			// length in dw ignoring dw - 2
			0);
		// 64-byte aligned offset of BLEND_STATE * 8 from Dynamic State Base Address
		ring.add_dw(
			blend_state_offset |
			// blend state pointer valid
			1);

		// setup raster
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_RASTER |
			// length in dw ignoring dw - 2
			3);
		ring.add_dw(
			// scissor rect enable
			0 << 1 |
			// cull mode none
			1 << 16 |
			// front winding counter-clockwise
			1 << 21 |
			// api mode dx10.1+
			2 << 22);
		// global depth offset constant (ieee float)
		ring.add_dw(0);
		// global depth offset scale (ieee float)
		ring.add_dw(0);
		// global depth offset clamp (ieee float)
		ring.add_dw(0);

		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_CLIP |
			// length in dw ignoring dw - 2
			2);
		ring.add_dw(0);
		ring.add_dw(0);
		ring.add_dw(0);

		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_SF |
			// length in dw ignoring dw - 2
			2);
		ring.add_dw(
			// 29:12 == line width in U11.7 format
			0 << 12 |
			// viewport transform enable
			0 << 1);
		ring.add_dw(0);
		ring.add_dw(0);

		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_SBE |
			// length in dw ignoring dw - 2
			4);
		ring.add_dw(
			// 26:22 == Number of SF output attributes
			// 15:11 == vertex urb entry read length (in 256-bit register increments), must be >= 1 <= 16
			1 << 11
			// 10:5 == vertex urb entry read offset in 256-bit units
			);
		ring.add_dw(0);
		ring.add_dw(0);
		ring.add_dw(0);
		ring.add_dw(0);

		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_WM |
			// length in dw ignoring dw - 2
			0);
		ring.add_dw(0);

		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DSTATE_PIPELINED |
			_3d_subcmd::_3DSTATE_PS |
			// length in dw ignoring dw - 2
			10);
		// 64-byte aligned Kernel Start Pointer 0 (offset from Instruction Base Address)
		ring.add_dw(pixel_shader_gtt_offset);
		ring.add_dw(pixel_shader_gtt_offset >> 32);
		ring.add_dw(
			// 31 == Single program flow (0 == SIMD n x m (multiple, m > 1), 1 == SIMD n x 1 (single))
			0);
		ring.add_dw(
			// 31:10 scratch space base pointer (1k aligned from general state base address)
			// skip the vertex shader space
			1024 |
			// each slice has 4 slines allowed

			// 3:0 per thread scratch space (0 == 1k, 11 = 2mb)
			0);
		// reserved
		ring.add_dw(0);
		ring.add_dw(0);
		ring.add_dw(
			// 22:16 == GRF constant/setup data 0 start register for kernel 0
			// 14:8 == GRF constant/setup data 1 start register for kernel 1
			// 0:6 == GRF constant/setup data2 start register for kernel 2
			0);
		// 64-byte aligned Kernel Start Pointer 1 (offset from Instruction Base Address)
		ring.add_dw(0);
		ring.add_dw(0);
		// 64-byte aligned Kernel Start Pointer 2 (offset from Instruction Base Address)
		ring.add_dw(0);
		ring.add_dw(0);

		// dispatch primitive using 3DPRIMITIVE
		ring.add_dw(
			cmd_type::GFX_PIPE |
			cmd_subtype::GFX_PIPE_3D |
			_3d_cmd::_3DPRIMITIVE |
			// length in dw ignoring dw - 2
			5);
		ring.add_dw(
			// vertex access type sequential
			0 << 8);
		// vertex count per instance
		ring.add_dw(3);
		// start vertex location
		ring.add_dw(0);
		// instance count
		ring.add_dw(1);
		// start instance location
		ring.add_dw(0);
		// base vertex location
		ring.add_dw(0);

		usize ring_buffer_gtt_start = ggtt_alloc(1);
		assert(ring_buffer_gtt_start);
		ggtt_map(ring_buffer_gtt_start, ring.page);

		// ring buffer length in 4kb units - 1
		ctx.regs[CTX_RING_BUFFER_CONTROL] =
			ring_buffer_ctl::BUFFER_LENGTH(0) |
			ring_buffer_ctl::ENABLE(true);

		ctx.regs[CTX_RING_BUFFER_START] = ring_buffer_gtt_start;

		// points to the qword after valid instruction data (may need to be padded)
		ctx.regs[CTX_RING_TAIL_POINTER] = ring.finish_get_tail();

		BitValue<u64> ctx_desc {
			context_descriptor::VALID(true) |
			context_descriptor::FAULT_HANDLING(context_descriptor::FAULT_AND_HANG) |
			context_descriptor::LRCA(logical_ctx_gtt_start >> 12) |
			context_descriptor::CTX_ID_ENGINE_INSTANCE(engine_instance::RCS) |
			context_descriptor::CTX_ID_ENGINE_CLASS(engine_class::RENDER)};
		space.store(regs::EXECLIST_SUBMITPORT_RCSUNIT, ctx_desc);
		space.store(regs::EXECLIST_SUBMITPORT_RCSUNIT, ctx_desc >> 32);

		println("trigger load of ctx");

		// trigger load of context
		space.store(regs::EXECLIST_CONTROL_RCSUNIT, execlist_control::LOAD(true));

		println("done");

		while (true) {
			asm volatile("hlt");
		}
	}

	usize ggtt_alloc(usize pages) {
		for (usize i = 8; i < gtt_size; i += 8) {
			if (!(gtt_space.load<u64>(i) & GTT_PRESENT)) {
				bool found = false;
				for (usize j = 0; j < pages; ++j) {
					if (gtt_space.load<u64>(i + j * 8) & GTT_PRESENT) {
						found = true;
						break;
					}
				}

				if (!found) {
					return i / 8 * 0x1000;
				}
			}
		}

		return 0;
	}

	void ggtt_map(usize gpu_virt, usize host_phys) {
		auto entry = gtt_space.load<u64>(gpu_virt / 0x1000 * 8);
		assert(!(entry & GTT_PRESENT));
		assert(!(host_phys & 0xFFF));
		gtt_space.store<u64>(gpu_virt / 0x1000 * 8, host_phys | GTT_PRESENT);
	}

	IoSpace space;
	IoSpace gtt_space;
	usize gtt_size;
};

static InitStatus intel_init(pci::Device& device) {
	IntelGpu gpu {device};

	return InitStatus::Success;
}

static constexpr PciDriver INTEL_DRIVER {
	.init = intel_init,
	.match = PciMatch::Device,
	.devices {
		{.vendor = 0x8086, .device = 0x9A40, .data = const_cast<Generation*>(&GEN11)},
		{.vendor = 0x8086, .device = 0x9A49, .data = const_cast<Generation*>(&GEN11)},
		{.vendor = 0x8086, .device = 0xA780, .data = const_cast<Generation*>(&GEN11)},
		// Intel UHD Graphics 620
		{.vendor = 0x8086, .device = 0x3EA0, .data = const_cast<Generation*>(&GEN7)}
	}
};

PCI_DRIVER(INTEL_DRIVER);
