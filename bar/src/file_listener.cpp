#include "main.hpp"
#include "src/config.hpp"
#include "file_listener.hpp"
#include <fcntl.h>
#include <list>
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
std::array<FileListener, sizeof(displayConfigs) / sizeof(displayConfigs[0])> setupFileListeners(std::list<Monitor> &mons, int inotify_fd)
{


	const auto brightnessCallback1 = [&]()
	{
		std::string buf;
		std::ifstream f(displayConfigs[0].first.c_str());
		f >> buf;
		size_t curBrightness = std::stoull(buf.c_str());
		for (auto &m : mons) {
			m.bar.setBrightness(curBrightness, 0);
			m.bar.invalidate();
		}
	};

	const auto brightnessCallback2 = [&]()
	{
		std::string buf;
		std::ifstream f(displayConfigs[1].first.c_str());
		f >> buf;
		size_t curBrightness = std::stoull(buf.c_str());
		for (auto &m : mons) {
			m.bar.setBrightness(curBrightness, 1);
			m.bar.invalidate();
		}
	};

	const std::array<FileListener, sizeof(displayConfigs) / sizeof(displayConfigs[0])> listeners = {
		FileListener(displayConfigs[0].first, brightnessCallback1, inotify_fd),
	};

	return listeners;
}
