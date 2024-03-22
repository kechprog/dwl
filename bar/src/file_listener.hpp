#pragma once
#include <filesystem>
#include <functional>
#include <sys/inotify.h>


class FileListener {
	public:
		FileListener(const std::filesystem::path file, std::function<void(void)> callback, int inotify_fd, int flags = IN_MODIFY);
		~FileListener();
		void operator()(const inotify_event *event) const;

	private:
		int flags;
		int watch_fd;
		int inotify_fd;
		std::function<void(void)> callback;
};

/* the big boy function */
std::vector<FileListener> setupFileListeners(int notify_fd);
