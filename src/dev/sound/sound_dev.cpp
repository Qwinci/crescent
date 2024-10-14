#include "sound_dev.hpp"
#include "sys/user_access.hpp"

int SoundDevice::handle_request(const kstd::vector<u8>& data, kstd::vector<u8>& res, usize max_size) {
	if (data.size() < sizeof(SoundLink) || max_size < sizeof(SoundLinkResponse)) {
		return ERR_INVALID_ARGUMENT;
	}
	auto* link = reinterpret_cast<const SoundLink*>(data.data());
	res.resize(sizeof(SoundLinkResponse));
	auto* resp = reinterpret_cast<SoundLinkResponse*>(res.data());

	switch (link->op) {
		case SoundLinkOp::GetInfo:
		{
			SoundDeviceInfo info {};
			if (auto err = get_info(info)) {
				return err;
			}
			resp->info.output_count = info.output_count;
			break;
		}
		case SoundLinkOp::GetOutputInfo:
		{
			if (auto err = get_output_info(link->get_output_info.index, resp->output_info)) {
				return err;
			}
			break;
		}
		case SoundLinkOp::SetActiveOutput:
			if (auto err = set_active_output(link->set_active_output.id)) {
				return err;
			}
			break;
		case SoundLinkOp::SetOutputParams:
		{
			SoundOutputParams params = link->set_output_params.params;
			if (auto err = set_output_params(params)) {
				return err;
			}
			resp->set_output_params.actual = params;
			break;
		}
		case SoundLinkOp::QueueOutput:
		{
			auto size = link->queue_output.len;

			kstd::vector<u8> buffer;
			buffer.resize(size);
			if (!UserAccessor(link->queue_output.buffer).load(buffer.data(), size)) {
				return ERR_FAULT;
			}

			if (auto err = queue_output(buffer.data(), size)) {
				return err;
			}
			break;
		}
		case SoundLinkOp::Play:
			if (auto err = play(link->play.play)) {
				return err;
			}
			break;
		case SoundLinkOp::Reset:
			if (auto err = reset()) {
				return err;
			}
			break;
		case SoundLinkOp::WaitUntilConsumed:
			if (auto err = wait_until_consumed(link->wait_until_consumed.trip_size, resp->wait_until_consumed.remaining)) {
				return err;
			}
			break;
	}

	return 0;
}
