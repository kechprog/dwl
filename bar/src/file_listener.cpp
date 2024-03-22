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
std::vector<FileListener> setupFileListeners(int inotify_fd)
{
	std::vector<FileListener> listeners {};
	for (size_t i = 0; i < config::brightness::display_count; i++) {
		const auto &name  = config::brightness::per_display_info[i].first;
		const auto &max_b = config::brightness::per_display_info[i].second; 
		listeners.emplace_back(name, [=](){
			std::string buf;
			std::ifstream f(name.c_str());
			f >> buf;
			size_t cur = std::stoull(buf);
			state::brightnesses[i] = cur / (double)max_b * 100;
			state::render();
		}, inotify_fd);
	}

	return listeners;
}
