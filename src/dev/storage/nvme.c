#include "arch/cpu.h"
#include "arch/interrupts.h"
#include "arch/misc.h"
#include "dev/dev.h"
#include "mem/allocator.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"
#include "string.h"
#include "utils/math.h"

typedef struct {
	volatile u64 cap;
	volatile u32 vs;
	volatile u32 intms;
	volatile u32 intmc;
	volatile u32 cc;
	volatile u32 reserved;
	volatile u32 csts;
	volatile u32 nssr;
	volatile u32 aqa;
	volatile u64 asq;
	volatile u64 acq;
} NvmeRegs;
static_assert(sizeof(NvmeRegs) == 56);

#define CAP_MQES(cap) ((cap) & 0xFFFF)
#define CAP_DSTRD(cap) ((cap) >> 32 & 0xF)
#define CAP_CSS(cap) ((cap) >> 37 & 0xFF)

#define CAP_CSS_NO_IO (1 << 7)
#define CAP_CSS_IO (1 << 6)
#define CAP_CSS_NVM (1 << 0)

#define CC_IOCQES(value) ((u32) (value) << 20)
#define CC_IOSQES(value) ((u32) (value) << 16)
#define CC_MPS(value) ((u32) (value) << 7)
#define CC_CSS(value) ((u32) (value) << 4)
#define CC_EN (1)

#define CSTS_RDY (1 << 0)

#define AQA_ACQS(value) ((u32) (value) << 16)
#define AQA_ASQS(value) ((u32) (value))

#define NVME_ACQ_SIZE 2
#define NVME_ASQ_SIZE 2

typedef union {
	struct {
		u8 opc;
		u8 flags;
		u16 cid;
		u32 nsid;
		u32 cdw2[2];
		u64 mptr;
		u64 prp1;
		u64 prp2;
		u32 cdw10[6];
	} common;

	struct {
		u8 opc;
		u8 flags;
		u16 cid;
		u32 unused1[3];
		u64 mptr;
		u64 unused2[2];
		u16 sqid;
		u16 a_cid;
		u32 unused4[5];
	} abort;

	struct {
		u8 opc;
		u8 flags;
		u16 cid;
		u32 unused1[3];
		u64 mptr;
		u64 prp1;
		u64 unused2;
		u16 qid;
		u16 qsize;
		u16 queue_flags;
		u16 iv;
		u32 unused3[4];
	} create_io_comp_queue;

	struct {
		u8 opc;
		u8 flags;
		u16 cid;
		u32 unused1[3];
		u64 mptr;
		u64 prp1;
		u64 unused2;
		u16 qid;
		u16 qsize;
		u16 queue_flags;
		u16 cqid;
		u16 nvmsetid;
		u16 unused3;
		u32 unused4[3];
	} create_io_sub_queue;

	struct {
		u8 opc;
		u8 flags;
		u16 cid;
		u32 unused1[3];
		u64 mptr;
		u64 unused2[2];
		u16 qid;
		u16 unused3;
		u32 unused4[5];
	} delete_io_queue;

	struct {
		u8 opc;
		u8 flags;
		u16 cid;
		u32 nsid;
		u32 cdw2[2];
		u64 mptr;
		u64 prp1;
		u64 prp2;
		u16 cns;
		u16 cntid;
		u16 cns_id;
		u16 csi;
		u32 unused1[2];
		u32 uuid_index;
		u32 unused2;
	} identify;

	struct {
		u8 opc;
		u8 flags;
		u16 cid;
		u32 nsid;
		u32 unused1[2];
		u64 mptr;
		u64 prp1;
		u64 prp2;
		u64 slba;
		u16 nlb;
		u16 r_flags;
		u32 dsm;
		u32 unused2;
		u16 elbat;
		u16 elbatm;
	} rw;

	struct {
		u8 opc;
		u8 flags;
		u16 cid;
		u32 nsid;
		u32 unused1[2];
		u64 mptr;
		u64 prp1;
		u64 prp2;
		u8 fid;
		u8 unused2;
		u16 unused3; // bit 31 Save
		u32 cdw11[3];
		u32 uuid_index;
		u32 cdw15;
	} features;
} NvmeCmd;
static_assert(sizeof(NvmeCmd) == 64);

#define FEAT_NUM_QUEUES 0x7

#define OP_DELETE_IO_SUB_QUEUE 0
#define OP_CREATE_IO_SUB_QUEUE 1
#define OP_DELETE_IO_COMP_QUEUE 4
#define OP_CREATE_IO_COMP_QUEUE 5
#define OP_IDENTIFY 6
#define OP_ABORT 8
#define OP_SET_FEATURES 9
#define IO_OP_FLUSH 0
#define IO_OP_WRITE 1
#define IO_OP_READ 2

#define QUEUE_FLAG_PC (1 << 0)
#define COMP_QUEUE_FLAG_IEN (1 << 1)
#define SUB_QUEUE_FLAG_QPRIO_LOW (0b11 << 1)
#define SUB_QUEUE_FLAG_QPRIO_MED (0b10 << 1)
#define SUB_QUEUE_FLAG_QPRIO_HIGH (1 << 1)
#define SUB_QUEUE_FLAG_QPRIO_URGENT (0)

typedef struct {
	u32 dw0;
	u32 dw1;
	u16 sqhd;
	u16 sqid;
	u16 cid;
	u16 status;
} NvmeCompletionEntry;
static_assert(sizeof(NvmeCompletionEntry) == 16);

#define COMP_STATUS_P (1)

typedef struct {
	NvmeCmd* sub_queue;
	volatile NvmeCompletionEntry* comp_queue;
	volatile u32* comp_queue_head_doorbell;
	volatile u32* sub_queue_tail_doorbell;
	u16 sub_queue_head;
	u16 comp_queue_head;
	u16 qid;
	u16 size;
	u8 phase;
	u16 cid;
} NvmeQueue;

typedef struct {
	NvmeRegs* regs;
	u8 stride;
	u32 max_queue_entries;
	NvmeQueue admin_queue;
} NvmeController;

#define SUBMISSION_QUEUE_TAIL_DOORBELL(num, stride, regs) offset(regs, volatile u32*, 0x1000 + 2 * (num) * (stride))
#define COMPLETION_QUEUE_HEAD_DOORBELL(num, stride, regs) offset(regs, volatile u32*, 0x1000 + (2 * (num) + 1) * (stride))

#define IDENT_CNS_NAMESPACE 0
#define IDENT_CNS_CONTROLLER 1
#define IDENT_CNS_ACTIVE_NS 2

void nvme_init_queue(NvmeController* self, NvmeQueue* queue, u16 qid, u16 num_entries) {
	Page* sub_queue_page = pmalloc(ALIGNUP(num_entries * sizeof(NvmeCmd), PAGE_SIZE) / PAGE_SIZE);
	assert(sub_queue_page);
	queue->sub_queue = (NvmeCmd*) to_virt(sub_queue_page->phys);
	memset(queue->sub_queue, 0, num_entries * sizeof(NvmeCmd));

	Page* comp_queue_page = pmalloc(ALIGNUP(num_entries * sizeof(NvmeCompletionEntry), PAGE_SIZE) / PAGE_SIZE);
	assert(comp_queue_page);
	queue->comp_queue = (volatile NvmeCompletionEntry*) to_virt(comp_queue_page->phys);
	memset((void*) queue->comp_queue, 0, num_entries * sizeof(NvmeCompletionEntry));

	queue->sub_queue_head = 0;
	queue->comp_queue_head = 0;

	queue->sub_queue_tail_doorbell = SUBMISSION_QUEUE_TAIL_DOORBELL(qid, self->stride, self->regs);
	queue->comp_queue_head_doorbell = COMPLETION_QUEUE_HEAD_DOORBELL(qid, self->stride, self->regs);

	queue->qid = qid;
	queue->size = num_entries;
	queue->phase = 1;
}

void nvme_queue_submit(NvmeQueue* self, NvmeCmd* cmd) {
	self->sub_queue[self->sub_queue_head++] = *cmd;

	if (self->sub_queue_head == self->size) {
		self->sub_queue_head = 0;
	}

	*self->sub_queue_tail_doorbell = self->sub_queue_head;
}

void nvme_queue_submit_and_wait(NvmeQueue* self, NvmeCmd* cmd) {
	cmd->common.cid = self->cid++;
	nvme_queue_submit(self, cmd);
	u16 status;
	while (true) {
		status = self->comp_queue[self->comp_queue_head].status;
		if ((status & COMP_STATUS_P) == self->phase) {
			break;
		}
	}

	assert((status >> 1) == 0 && "nvme command failed");

	++self->comp_queue_head;
	if (self->comp_queue_head == self->size) {
		self->comp_queue_head = 0;
		self->phase ^= 1;
	}

	*self->comp_queue_head_doorbell = self->comp_queue_head;
}

typedef struct {
	u16 vid;
	u16 ssvid;
	char sn[20];
	char mn[40];
	char fr[8];
	u8 rab;
	u8 ieee[3];
	u8 cmic;
	u8 mdts;
	u16 cntlid;
	u32 ver;
	u32 rtd3r;
	u32 rtd3e;
	u32 oaes;
	u32 ctratt;
	u16 rrls;
	u8 reserved1[9];
	u8 cntrltype;
	u8 fguid[16];
	u16 crdt1;
	u16 crdt2;
	u16 crdt3;
	u8 reserved2[119];
	u8 nvmsr;
	u8 vwci;
	u8 mec;
	u16 oacs;
	u8 acl;
	u8 aerl;
	u8 frmw;
	u8 lpa;
	u8 elpe;
	u8 npss;
	u8 avscc;
	u8 apsta;
	u16 wctemp;
	u16 cctemp;
	u16 mtfa;
	u32 hmpre;
	u32 hmmin;
	u8 tnvmcap[16];
	u8 unvmcap[16];
	u32 rpmbs;
	u16 edstt;
	u8 dsto;
	u8 fwug;
	u16 kas;
	u16 hctma;
	u16 mntmt;
	u16 mxtmt;
	u32 sanicap;
	u32 hmminds;
	u16 hmmaxd;
	u16 nsetidmax;
	u16 endgidmax;
	u8 anatt;
	u8 anacap;
	u32 anagrpmax;
	u32 nanagrpid;
	u32 pels;
	u16 domain_ident;
	u8 reserved3[10];
	u8 megcap[16];
	u8 reserved4[128];
	u8 sqes;
	u8 cqes;
	u16 maxcmd;
	u32 nn;
	u16 oncs;
	u16 fuses;
	u8 fna;
	u8 vwc;
	u16 awun;
	u16 awupf;
	u8 icsvscc;
	u8 nwpc;
	u16 acwu;
	u16 copy_desc_fmts_supported;
	u32 sgls;
	u32 mnan;
	u8 maxdna[16];
	u32 maxcna;
	u8 reserved5[204];
	char subnqn[256];
	u8 reserved6[768 + 256];
	u8 psds[32 * 32];
	u8 vendor[1024];
} NvmeIdentifyController;

typedef struct {
	u16 ms;
	u8 lbads;
	u8 rp_2bit;
} NvmeLbaFormat;

typedef struct {
	u64 nsze;
	u64 ncap;
	u64 nuse;
	u8 nsfeat;
	u8 nlbaf;
	u8 flbas;
	u8 mc;
	u8 dpc;
	u8 dps;
	u8 nmic;
	u8 rescap;
	u8 fpi;
	u8 dlfeat;
	u16 nawun;
	u16 nawupf;
	u16 nacwu;
	u16 nabsn;
	u16 nabo;
	u16 nabspf;
	u16 noiob;
	u8 nvmcap[16];
	u16 npwg;
	u16 npwa;
	u16 npdg;
	u16 npda;
	u16 nows;
	u16 mssrl;
	u32 mcl;
	u8 msrc;
	u8 reserved1[11];
	u32 anagrpid;
	u8 reserved2[3];
	u8 nsattr;
	u16 nvmsetid;
	u16 endgid;
	u8 nguid[16];
	u64 eui64;
	NvmeLbaFormat lbafs[64];
	u8 vendor[3712];
} NvmeIdentifyNamespace;

void nvme_identify(NvmeController* self, NvmeIdentifyController* ident) {
	NvmeCmd cmd = {};
	cmd.identify.opc = OP_IDENTIFY;
	cmd.identify.nsid = 0;
	cmd.identify.prp1 = to_phys(ident);
	cmd.identify.cns = IDENT_CNS_CONTROLLER;
	nvme_queue_submit_and_wait(&self->admin_queue, &cmd);
}

void nvme_identify_ns(NvmeController* self, u32 nsid, NvmeIdentifyNamespace* ident) {
	NvmeCmd cmd = {};
	cmd.identify.opc = OP_IDENTIFY;
	cmd.identify.nsid = nsid;
	cmd.identify.prp1 = to_phys(ident);
	cmd.identify.cns = IDENT_CNS_NAMESPACE;
	nvme_queue_submit_and_wait(&self->admin_queue, &cmd);
}

typedef struct {
	NvmeQueue queue;
} NvmeNamespace;

void nvme_ns_create_queues(NvmeController* controller, NvmeNamespace* ns, u16 qid) {
	nvme_init_queue(controller, &ns->queue, qid, PAGE_SIZE / sizeof(NvmeCmd));

	NvmeCmd create_comp_queue_cmd = {};
	create_comp_queue_cmd.create_io_comp_queue.opc = OP_CREATE_IO_COMP_QUEUE;
	create_comp_queue_cmd.create_io_comp_queue.prp1 = to_phys((void*) ns->queue.comp_queue);
	create_comp_queue_cmd.create_io_comp_queue.qid = qid;
	create_comp_queue_cmd.create_io_comp_queue.qsize = PAGE_SIZE / sizeof(NvmeCmd) - 1;
	create_comp_queue_cmd.create_io_comp_queue.queue_flags = QUEUE_FLAG_PC;
	create_comp_queue_cmd.create_io_comp_queue.iv = 0;
	nvme_queue_submit_and_wait(&controller->admin_queue, &create_comp_queue_cmd);

	NvmeCmd create_sub_queue_cmd = {};
	create_sub_queue_cmd.create_io_sub_queue.opc = OP_CREATE_IO_SUB_QUEUE;
	create_sub_queue_cmd.create_io_sub_queue.prp1 = to_phys(ns->queue.sub_queue);
	create_sub_queue_cmd.create_io_sub_queue.qid = qid;
	create_sub_queue_cmd.create_io_sub_queue.cqid = qid;
	create_sub_queue_cmd.create_io_sub_queue.qsize = PAGE_SIZE / sizeof(NvmeCmd) - 1;
	create_sub_queue_cmd.create_io_sub_queue.queue_flags = QUEUE_FLAG_PC | SUB_QUEUE_FLAG_QPRIO_MED;
	nvme_queue_submit_and_wait(&controller->admin_queue, &create_sub_queue_cmd);
}

void nvme_init_namespace(NvmeController* self, u32 nsid) {
	NvmeIdentifyNamespace* ident_ns = (NvmeIdentifyNamespace*) kmalloc(sizeof(NvmeIdentifyNamespace));
	assert(ident_ns);
	memset(ident_ns, 0, sizeof(NvmeIdentifyNamespace));
	nvme_identify_ns(self, nsid, ident_ns);
	kprintf("[kernel][nvme]: identified namespace with id %u\n", nsid);

	u8 fmt_index = ident_ns->flbas & 0xF;
	if (ident_ns->nlbaf > 16) {
		fmt_index |= (ident_ns->flbas >> 5 & 0b11) << 4;
	}
	usize lba_size = 1 << ident_ns->lbafs[fmt_index].lbads;
	usize lba_count = ident_ns->nsze;

	NvmeNamespace* ns = (NvmeNamespace*) kmalloc(sizeof(NvmeNamespace));
	assert(ns);

	nvme_ns_create_queues(self, ns, nsid);
	kprintf("[kernel][nvme]: successfully created io queues for ns %u\n", nsid);

	kprintf("[kernel][nvme]: ns %u has %u blocks of size %u (total %uGB)\n", nsid, lba_count, lba_size, lba_count * lba_size / 1024 / 1024 / 1024);

	u8* test_data = kmalloc(lba_size);
	assert(test_data);

	NvmeCmd read_cmd = {};
	read_cmd.rw.opc = IO_OP_READ;
	read_cmd.rw.nsid = nsid;
	read_cmd.rw.prp1 = to_phys(test_data);
	// starting lba
	read_cmd.rw.slba = 0;
	// Number of lbas - 1
	read_cmd.rw.nlb = 0;
	nvme_queue_submit_and_wait(&ns->queue, &read_cmd);

	kfree(test_data, lba_size);
	kfree(ident_ns, sizeof(NvmeIdentifyNamespace));
}

static void nvme_init(PciHdr0* hdr) {
	if (pci_is_io_space(hdr, 0)) {
		return;
	}
	void* base = to_virt(pci_map_bar(hdr, 0));
	NvmeRegs* regs = (NvmeRegs*) base;

	regs->cc &= ~CC_EN;
	while (regs->csts & CSTS_RDY) {
		arch_spinloop_hint();
	}

	regs->aqa = AQA_ACQS(MIN(2048, PAGE_SIZE / sizeof(NvmeCompletionEntry))) | AQA_ASQS(MIN(2048, PAGE_SIZE / sizeof(NvmeCmd)));

	NvmeController* self = kmalloc(sizeof(NvmeController));
	assert(self);
	self->regs = regs;
	self->max_queue_entries = CAP_MQES(regs->cap) + 1;
	self->stride = 4U << CAP_DSTRD(regs->cap);

	nvme_init_queue(self, &self->admin_queue, 0, MIN(2048, PAGE_SIZE / sizeof(NvmeCmd)));
	regs->acq = to_phys((void*) self->admin_queue.comp_queue);
	regs->asq = to_phys(self->admin_queue.sub_queue);

	regs->cc = CC_EN | CC_IOSQES(/*2 ^ 6 == 64*/ 6) | CC_IOCQES(/*2 ^ 4 == 16*/ 4);

	while (!(regs->csts & CSTS_RDY)) {
		arch_spinloop_hint();
	}

	kprintf("[kernel][nvme]: init\n");

	NvmeIdentifyController* ident_controller = (NvmeIdentifyController*) kmalloc(sizeof(NvmeIdentifyController));
	assert(ident_controller);
	memset(ident_controller, 0, sizeof(NvmeIdentifyController));
	nvme_identify(self, ident_controller);
	kprintf("[kernel][nvme]: controller ident success\n");

	NvmeCmd set_queue_count_cmd = {};
	set_queue_count_cmd.features.opc = OP_SET_FEATURES;
	set_queue_count_cmd.features.fid = FEAT_NUM_QUEUES;
	set_queue_count_cmd.features.cdw11[0] = (4 - 1) | ((4 - 1) << 16);
	nvme_queue_submit_and_wait(&self->admin_queue, &set_queue_count_cmd);
	kprintf("[kernel][nvme]: set io queue count to 4\n");

	// todo more than 1024 active namespaces

	u32* active_ns_list = (u32*) kmalloc(0x1000);
	assert(active_ns_list);
	memset(active_ns_list, 0, 0x1000);

	NvmeCmd active_ns_cmd = {};
	active_ns_cmd.identify.opc = OP_IDENTIFY;
	active_ns_cmd.identify.nsid = 0;
	active_ns_cmd.identify.cns = IDENT_CNS_ACTIVE_NS;
	active_ns_cmd.identify.prp1 = to_phys(active_ns_list);
	nvme_queue_submit_and_wait(&self->admin_queue, &active_ns_cmd);

	for (u32 i = 0; i < MIN(ident_controller->nn, 1024); ++i) {
		if (!active_ns_list[i]) {
			break;
		}
		nvme_init_namespace(self, active_ns_list[i]);
	}

	kfree(ident_controller, sizeof(NvmeIdentifyController));
	kfree(active_ns_list, 0x1000);
}

static PciDriver nvme_pci_driver = {
	.match = PCI_MATCH_CLASS | PCI_MATCH_SUBCLASS | PCI_MATCH_PROG,
	.dev_class = 1,
	.dev_subclass = 8,
	.dev_prog = 2,
	.load = nvme_init
};

PCI_DRIVER(nvme, &nvme_pci_driver);