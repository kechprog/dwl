#include "dbus_handles.hpp"
#include "dbus/dbus-shared.h"
#include <iostream>
#include <optional>
#include <ostream>
#include <sys/poll.h>
#include <utility>

DbusListener::DbusListener(void)
{
	dbus_error_init(&err);
	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

	if (dbus_error_is_set(&err)) {
		std::cerr << "Unable to initialize bus, reason: " << err.message << std::endl;
		exit(-1);
	}

    dbus_bus_add_match(conn, rule, &err);
    dbus_connection_flush(conn);
    if (dbus_error_is_set(&err)) { 
		std::cerr << "Unable to set rule: " << err.message << std::endl;
		exit(-1);
    }

	dbus_connection_get_unix_fd(conn, &fd);
}

DbusListener::~DbusListener()
{
	/* todo */
}

int DbusListener::get_fd(void) const
{
	return fd;
}

void DbusListener::operator()(short int revents, std::list<Monitor> &mons) const
{
		if (!(revents & POLLIN))
			return;

		dbus_connection_read_write(conn, 0);
		DBusMessage* msg;
		while ((msg = dbus_connection_pop_message(conn)) != NULL) {
			auto [brightness, status] = parse_msg(msg);
			if (brightness.has_value()) for (auto &m : mons) {
				std::cout << "Battery: " << brightness.value() << std::endl;
				m.bar.setBat(brightness.value(), true);
			}
			dbus_message_unref(msg);
		}
}

std::pair<std::optional<double>, std::optional<bool>>
DbusListener::parse_msg(DBusMessage *msg) const
{
	if (!dbus_message_is_signal(msg, "org.freedesktop.DBus.Properties", "PropertiesChanged"))
		return {std::nullopt, std::nullopt};	

	const char* interface_name;
	DBusMessageIter args, dict;
	dbus_message_iter_init(msg, &args);
	dbus_message_iter_get_basic(&args, &interface_name);

	std::optional<double> ret_perc = std::nullopt; 
	std::optional<bool> ret_status = std::nullopt;

	if (strcmp("org.freedesktop.UPower.Device", interface_name) == 0) {
		dbus_message_iter_next(&args);
		dbus_message_iter_recurse(&args, &dict);

		while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
			DBusMessageIter entry;
			dbus_message_iter_recurse(&dict, &entry);

			const char* key;
			dbus_message_iter_get_basic(&entry, &key);

			/* match property name */
			if (strcmp("Percentage", key) == 0) {
				dbus_message_iter_next(&entry);
				DBusMessageIter var;
				dbus_message_iter_recurse(&entry, &var);
				double val;
				dbus_message_iter_get_basic(&var, &val);
				ret_perc = val;
				std::cout << "Lowest level, battery: " << val << std::endl;
			}

			dbus_message_iter_next(&dict);
		}
	}
	return {ret_perc, ret_status};
}
