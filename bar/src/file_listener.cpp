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
	const auto brightnessCallback1 = [&]()
	{
		std::string buf;
		std::ifstream f(config::brightness::per_display_info[0].first.c_str());
		f >> buf;
		size_t curBrightness = std::stoull(buf.c_str());
		state::brightnesses[0] = curBrightness / (double) config::brightness::per_display_info[0].second * 100;
		state::render();
	};

	const auto brightnessCallback2 = [&]()
	{
		std::string buf;
		std::ifstream f(config::brightness::per_display_info[1].first.c_str());
		f >> buf;
		size_t curBrightness = std::stoull(buf.c_str());

		state::brightnesses[1] = curBrightness / (double) config::brightness::per_display_info[1].second * 100;
		state::render();
	};

	const std::array<FileListener, config::brightness::display_count> listeners = {
		FileListener(config::brightness::per_display_info[0].first, brightnessCallback1, inotify_fd),
		FileListener(config::brightness::per_display_info[1].first, brightnessCallback2, inotify_fd),
	};

	return listeners;
}
