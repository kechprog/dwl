#include "Event.hpp"
#include "src/common.hpp"
#include <unistd.h>

static int event_pipe = {0};

int event_queue_init() {
	int fds[2];
	if (pipe(fds) < 0) {
		die("Could not create event pipe");
	}

	event_pipe = fds[1];

	return fds[0];
}

void event_enqueue(std::function<void(void*)> cb, void* data) {
	Event ev(cb, data);

	write(event_pipe, &ev, sizeof(ev));
}

void Event::operator()() const {
	this->cb(this->data);
}
