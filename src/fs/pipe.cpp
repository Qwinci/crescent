#include "pipe.hpp"

PipeVNode::PipeVNode(kstd::shared_ptr<RingBuffer<u8>> buffer, bool reading)
	: buffer {std::move(buffer)}, reading {reading} {}

kstd::optional<kstd::pair<PipeVNode, PipeVNode>> PipeVNode::create(usize max_size) {
	auto buffer = kstd::make_shared<RingBuffer<u8>>(max_size);
	if (!buffer) {
		return {};
	}

	PipeVNode reading {buffer, true};
	PipeVNode writing {std::move(buffer), false};

	return {{std::move(reading), std::move(writing)}};
}

FsStatus PipeVNode::read(void* data, usize size, usize offset) {
	if (!reading || offset) {
		return FsStatus::Unsupported;
	}

	buffer->read_block(data, size);

	return FsStatus::Success;
}

FsStatus PipeVNode::write(const void* data, usize size, usize offset) {
	if (reading || offset) {
		return FsStatus::Unsupported;
	}

	buffer->write_block(data, size);

	return FsStatus::Success;
}

FsStatus PipeVNode::stat(FsStat& data) {
	data.size = buffer->size();

	return FsStatus::Success;
}
