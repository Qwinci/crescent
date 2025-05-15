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
		case SoundLinkOpGetInfo:
		{
			SoundDeviceInfo info {};
			if (auto err = get_info(info)) {
				return err;
			}
			resp->info.output_count = info.output_count;
			break;
		}
		case SoundLinkOpGetOutputInfo:
		{
			if (auto err = get_output_info(link->get_output_info.index, resp->output_info)) {
				return err;
			}
			break;
		}
		case SoundLinkOpSetActiveOutput:
			if (auto err = set_active_output(link->set_active_output.id)) {
				return err;
			}
			break;
		case SoundLinkOpSetOutputParams:
		{
			SoundOutputParams params = link->set_output_params.params;
			if (auto err = set_output_params(params)) {
				return err;
			}
			resp->set_output_params.actual = params;
			break;
		}
		case SoundLinkOpQueueOutput:
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
		case SoundLinkOpPlay:
			if (auto err = play(link->play.play)) {
				return err;
			}
			break;
		case SoundLinkOpWaitUntilConsumed:
			if (auto err = wait_until_consumed(link->wait_until_consumed.trip_size, resp->wait_until_consumed.remaining)) {
				return err;
			}
			break;
	}

	return 0;
}
