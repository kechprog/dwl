#include "main.hpp"
#include "src/config.hpp"
#include "file_listener.hpp"
#include <fcntl.h>
#include <iostream>
#include <list>
#include <sys/inotify.h>
#include <fstream>

FileListener::FileListener
	(const std::string_view file_path, std::function<void(void)> callback, const int inotify_fd, int flags)
{
	this->watch_fd = inotify_add_watch(inotify_fd, file_path.data(), flags);
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

constexpr int FileListener::get_watch_fd() const
{
	return watch_fd;
}

/* compare with watch_fd */
bool FileListener::operator==(const int watch_fd) const
{
	return this->watch_fd == watch_fd;
}

/* call callback */
void FileListener::operator()(const inotify_event *event) const
{
	if (event->mask & this->flags)
		this->callback();

}

/* defenition of some listeners */
std::array<FileListener, 2> setupFileListeners(std::list<Monitor> &mons, int inotify_fd)
{

	const auto batCallback = [&]()
	{
		std::string buf;
		std::ifstream f(batChargeNow.data());
		
		f >> buf;
		size_t curCharge = std::stoull(buf.c_str());

		for (auto &m: mons) {
			m.bar.setBat((double)curCharge / (double)batChargeFull * 100, true);
		}
	};

	const auto brightnessCallback1 = [&]()
	{
		std::string buf;
		std::ifstream f(displayConfigs[0].first.data());
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
		std::ifstream f(displayConfigs[1].first.data());
		f >> buf;
		size_t curBrightness = std::stoull(buf.c_str());
		for (auto &m : mons) {
			m.bar.setBrightness(curBrightness, 1);
			m.bar.invalidate();
		}
	};

	const std::array<FileListener, 2> listeners = {
		// FileListener(batChargeNow, batCallback, inotify_fd),
		FileListener(displayConfigs[0].first, brightnessCallback1, inotify_fd),
		FileListener(displayConfigs[1].first, brightnessCallback2, inotify_fd),
	};

	return listeners;
}
