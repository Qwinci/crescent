#include "pipe.hpp"

PipeVNode::PipeVNode(kstd::shared_ptr<RingBuffer<u8>> buffer, FileFlags flags, bool reading)
	: VNode {flags, false}, buffer {std::move(buffer)}, reading {reading} {}

kstd::optional<kstd::pair<PipeVNode, PipeVNode>> PipeVNode::create(usize max_size, FileFlags read_flags, FileFlags write_flags) {
	auto buffer = kstd::make_shared<RingBuffer<u8>>(max_size);
	if (!buffer) {
		return {};
	}

	PipeVNode reading {buffer, read_flags, true};
	PipeVNode writing {std::move(buffer), write_flags, false};

	return {{std::move(reading), std::move(writing)}};
}

FsStatus PipeVNode::read(void* data, usize& size, usize offset) {
	if (!reading || offset) {
		return FsStatus::Unsupported;
	}

	if (flags & FileFlags::NonBlock) {
		size = buffer->read(data, size);
		if (!size) {
			return FsStatus::TryAgain;
		}
		return FsStatus::Success;
	}
	else {
		buffer->read_block(data, size);
		return FsStatus::Success;
	}
}

FsStatus PipeVNode::write(const void* data, usize& size, usize offset) {
	if (reading || offset) {
		return FsStatus::Unsupported;
	}

	if (flags & FileFlags::NonBlock) {
		size = buffer->write(data, size);
		if (!size) {
			return FsStatus::TryAgain;
		}
		return FsStatus::Success;
	}
	else {
		buffer->write_block(data, size);
		return FsStatus::Success;
	}
}

FsStatus PipeVNode::stat(FsStat& data) {
	data.size = buffer->size();
	return FsStatus::Success;
}
