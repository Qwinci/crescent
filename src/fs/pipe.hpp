#pragma once
#include "vfs.hpp"
#include "vector.hpp"
#include "optional.hpp"
#include "dev/event.hpp"
#include "ring_buffer.hpp"

class PipeVNode : public VNode {
public:
	static kstd::optional<kstd::pair<PipeVNode, PipeVNode>> create(usize max_size, FileFlags read_flags, FileFlags write_flags);

	FsStatus read(void* data, usize& size, usize offset) override;
	FsStatus write(const void* data, usize& size, usize offset) override;
	FsStatus stat(FsStat& data) override;

private:
	PipeVNode(kstd::shared_ptr<RingBuffer<u8>> buffer, FileFlags flags, bool reading);

	kstd::shared_ptr<RingBuffer<u8>> buffer;
	bool reading = false;
};
