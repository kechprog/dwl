#pragma once
#include "src/main.hpp"
#include <functional>
#include <string_view>
#include <sys/inotify.h>
#include <list>
#include <array>


class FileListener {
	public:
		FileListener(const std::string_view file_path, std::function<void(void)> callback, int inotify_fd, int flags = IN_MODIFY);
		~FileListener();
		
		constexpr int get_watch_fd() const;
		bool operator==(const int watch_fd) const;	
		void operator()(const inotify_event *event) const;

	private:
		int flags;
		int watch_fd;
		int inotify_fd;
		std::function<void(void)> callback;
};

/* the big boy function */
std::array<FileListener, 3> setupFileListeners(std::list<Monitor> &mons, int notify_fd);
