#pragma once

#include "src/main.hpp"
#include <dbus-1.0/dbus/dbus.h>
#include <list>

// /* returns fd */
// int dbus_setup(DBusConnection **conn);
//
// /* on POLLIN on fd */
// void dbus_on_poll(std::list<Monitor> &mons);

class DbusListener 
{
	public:
		DbusListener(void);
		~DbusListener();
		int get_fd(void) const;
		/* to be called on any poll event */
		void operator()(short int revents, std::list<Monitor> &mons) const;

	private:
		std::pair<std::optional<double>, std::optional<bool>> parse_msg(DBusMessage *msg) const;

		int fd;
		DBusConnection *conn;
		DBusError err;
		static constexpr char rule[] = "type='signal',interface='org.freedesktop.DBus.Properties',"
									   "member='PropertiesChanged',"
									   "arg0namespace='org.freedesktop.UPower.Device'";
};
