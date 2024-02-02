#pragma once

#include <dbus-1.0/dbus/dbus.h>
#include <optional>
#include <utility>

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
		void operator()(short int revents) const;

	private:
		std::pair<std::optional<double>, std::optional<bool>> parse_msg(DBusMessage *msg) const;
		void make_initial_battery_state_request();

		int fd;
		DBusConnection *conn;
		DBusError err;
		static constexpr char rule[] = "type='signal',interface='org.freedesktop.DBus.Properties',"
									   "member='PropertiesChanged',"
									   "arg0namespace='org.freedesktop.UPower.Device'";
};
