#include "pa_vol.hpp"
#include "src/State.hpp"
#include <iostream>
#include <thread>
#include <unistd.h>

std::atomic<bool> PaListener::worker_running = false;
int PaListener::self_pipe[2] = {0};

void PaListener::worker_state_cb(pa_context* context, void* userdata)
{
    pa_mainloop_api* mainloop_api = reinterpret_cast<pa_mainloop_api*>(userdata);

    switch (pa_context_get_state(context)) {
	case PA_CONTEXT_READY: {
		pa_operation* op_subscribe = pa_context_subscribe(
			context, 
			(pa_subscription_mask_t)(PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE), /* c++ ... */
			nullptr, nullptr
		);
		if (op_subscribe) 
			pa_operation_unref(op_subscribe);

		// Query initial state of all sinks
		pa_operation* op_query_sink = pa_context_get_sink_info_list(
			context, 
			[](pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
				if (eol < 0 || !i) 
					return;

				PaEvent ev = {
					.type = PaEventType::Audio,
					.data = {
						.vol = {
						    .is_mute = i->mute == 1, 
						    .volume  = i->volume.values[0] * 100 / PA_VOLUME_NORM,
						},
					}
				};

				write(self_pipe[1], &ev, sizeof(ev));
			}, 
			nullptr
		);

		pa_operation *op_query_source = pa_context_get_source_info_list(
			context,
			[](pa_context *c, const pa_source_info *i, int eol, void *userdata) {
				if (eol < 0 || !i) 
					return;
				PaEvent ev = {.type = PaEventType::Mic};
				ev.data.mic_is_mute = (i->mute == 1);
				write(self_pipe[1], &ev, sizeof(ev));
			},
			nullptr
		);


		if (op_query_source) 
			pa_operation_unref(op_query_source);

		if (op_query_sink) 
			pa_operation_unref(op_query_sink);

		break;
        }
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            mainloop_api->quit(mainloop_api, 0);
            break;
        default:
            break;
    }
}

void PaListener::worker_sub_cb(pa_context* context, pa_subscription_event_type_t t, uint32_t index, void* userdata)
{
    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK) {
        pa_operation* op = pa_context_get_sink_info_by_index(
		context, 
		index,
		[](pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
			if (eol < 0 || !i)
				return;
			PaEvent ev = {
				.type = PaEventType::Audio,
				.data = {
					.vol = {
					    .is_mute = i->mute == 1, 
					    .volume  = i->volume.values[0] * 100 / PA_VOLUME_NORM,
					},
				}
			};

			write(self_pipe[1], &ev, sizeof(ev));
		}, 
		nullptr
	);


        if (op) 
	    pa_operation_unref(op);
    }

    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE) {
	pa_operation *op_query_source = pa_context_get_source_info_by_index(
		context,
		index,
		[](pa_context *c, const pa_source_info *i, int eol, void *userdata) {
			if (eol < 0 || !i)
				return;
			PaEvent ev = {.type = PaEventType::Mic};
			ev.data.mic_is_mute = (i->mute == 1);

			write(self_pipe[1], &ev, sizeof(ev));
		},
		nullptr
	);


	if (op_query_source) 
		pa_operation_unref(op_query_source);
    }
}

void PaListener::worker_main()
{
    pa_mainloop* m = pa_mainloop_new();
    pa_mainloop_api* mainloop_api = pa_mainloop_get_api(m);

    pa_context* context = pa_context_new(mainloop_api, "Volume Monitor");

    pa_context_set_state_callback(context, PaListener::worker_state_cb, mainloop_api);

    pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    pa_context_set_subscribe_callback(context, PaListener::worker_sub_cb, nullptr);

    int ret = 1;
    if (pa_mainloop_run(m, &ret) < 0) {
        std::cerr << "Failed to run mainloop." << std::endl;
    }

    pa_context_disconnect(context);
    pa_context_unref(context);
    pa_mainloop_free(m);
}

PaListener::PaListener()
{
	if (worker_running) return;
	pipe(self_pipe);
	std::thread(PaListener::worker_main).detach();
	worker_running = true;
}

int PaListener::get_fd() const
{
	return self_pipe[0];
}

void PaListener::operator() () const
{
	PaEvent ev;
	read(self_pipe[0], &ev, sizeof(ev));
	switch (ev.type) {
		case PaEventType::Audio: 	
			state::volume = ev.data.vol.volume;
			state::vol_is_mute = ev.data.vol.is_mute;
			break;

		case PaEventType::Mic: 	
			state::mic_is_mute = ev.data.mic_is_mute;
			break;
	}
	state::render();
}
