#include "src/config.hpp"
#include "State.hpp"
#include "file_listener.hpp"
#include <fcntl.h>
#include <string>
#include <sys/inotify.h>
#include <fstream>

FileListener::FileListener
	(const std::filesystem::path file, std::function<void(void)> callback, const int inotify_fd, int flags)
{
	this->watch_fd = inotify_add_watch(inotify_fd, file.c_str(), flags);
	this->inotify_fd = inotify_fd;
	this->callback = callback;
	this->flags = flags;

	/* set initial value */
	callback();
}

FileListener::~FileListener()
{
	inotify_rm_watch(inotify_fd, watch_fd);
}

/* call callback */
void FileListener::operator()(const inotify_event *event) const
{
	if (event->wd == this->watch_fd 
	&& (event->mask & this->flags))
		this->callback();

}

/* defenition of some listeners */
std::array<FileListener, config::brightness::display_count> setupFileListeners(int inotify_fd)
{
	std::array<FileListener, config::brightness::display_count> listeners = {};
	for (size_t i = 0; i < config::brightness::display_count; i++) {
		listeners[i] = FileListener(config::brightness::per_display_info[i].first, [i]() {
			std::string buf;
			std::ifstream f(config::brightness::per_display_info[i].first.c_str());
			f >> buf;
			size_t curBrightness = std::stoull(buf.c_str());
			state::brightnesses[0] = curBrightness / (double) config::brightness::per_display_info[i].second * 100;
			state::render();
		}, inotify_fd);
	}

	return listeners;
}
