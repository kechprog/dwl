#pragma once
#include <functional>

class Event {
public:
	Event(std::function<void(void*)> cb, void* data)
		: cb {cb}, data {data} {};
	void operator()() const;

private:
	std::function<void(void*)> cb;
	void* data;
};

int event_queue_init();
