#pragma once
#include <atomic>
#include <pulse/pulseaudio.h>

// TODO: query initial state

struct VolEvent {
	bool is_mute;
	uint32_t volume;
};

enum class PaEventType {
	Mic,
	Audio,
};

struct PaEvent {
	PaEventType type;
	union {
		VolEvent vol;
		bool mic_is_mute; /* mic state */
	} data;
};

class PaListener 
{
	static int self_pipe[2];
	static std::atomic<bool> worker_running;

	static void worker_state_cb(pa_context* context, void* userdata);
	static void worker_sub_cb(pa_context* context, pa_subscription_event_type_t t, uint32_t index, void* userdata);
	static void worker_main();

	public:
	PaListener();
	int get_fd() const;
	void operator() () const;
};
