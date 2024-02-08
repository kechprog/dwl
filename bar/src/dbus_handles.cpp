#include "dbus_handles.hpp"
#include "State.hpp"
#include "dbus/dbus-shared.h"
#include <iostream>
#include <optional>
#include <ostream>
#include <sys/poll.h>
#include <utility>


static void dbus_query_bat(DBusConnection *conn, const char *property, void *ret)
{
	DBusError err;
	const char* obj_path = config::battery::dbus_obj_path;
	const char* interface_name = "org.freedesktop.DBus.Properties";

	dbus_error_init(&err);

	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.UPower", // service to contact
		obj_path,                  // object path
		interface_name,            // interface name
		"Get");                   // method name

	DBusMessageIter args;
	dbus_message_iter_init_append(msg, &args);
	const char* prop_iface = "org.freedesktop.UPower.Device";
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &prop_iface)
	||  !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &property)) {
	std::cerr << "Out of Memory!" << std::endl;
	}

	DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
	if (!reply) {
	std::cerr << "Failed to send message: " << (err.message ? err.message : "unknown error") << std::endl;
	}

	DBusMessageIter replyIter;
	dbus_message_iter_init(reply, &replyIter);
	DBusMessageIter variantIter;
	if (dbus_message_iter_get_arg_type(&replyIter) == DBUS_TYPE_VARIANT) {
		dbus_message_iter_recurse(&replyIter, &variantIter);
		dbus_message_iter_get_basic(&variantIter, ret);
	}

	dbus_message_unref(reply);
	dbus_message_unref(msg);
}


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
	make_initial_battery_state_request();
}


DbusListener::~DbusListener()
{
	/* todo */
}


void DbusListener::make_initial_battery_state_request()
{
	double bat_perc;
	uint32_t bat_state;
	dbus_query_bat(this->conn, "Percentage", &bat_perc);
	dbus_query_bat(this->conn, "State", &bat_state);

	state::bat_percentage = bat_perc;
	state::bat_is_charging = bat_state != 2;
	std::cout << "Initial battery state: " << bat_perc << " " << bat_state << std::endl;
	state::render();
}


int DbusListener::get_fd(void) const
{
	return fd;
}


void DbusListener::operator()(short int revents) const
{
	if (!(revents & POLLIN))
		return;

	dbus_connection_read_write(conn, 0);
	DBusMessage* msg;
	while ((msg = dbus_connection_pop_message(conn)) != NULL) {
		auto [chrg, status] = parse_msg(msg);
		state::bat_percentage = chrg.value_or(state::bat_percentage);
		state::bat_is_charging = status.value_or(state::bat_is_charging);
		state::render();
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

			if (strcmp("State", key) == 0) {
				dbus_message_iter_next(&entry);
				DBusMessageIter var;
				dbus_message_iter_recurse(&entry, &var);
				uint32_t val;
				dbus_message_iter_get_basic(&var, &val);
				ret_status = val != 2; // is charging?
			}

			dbus_message_iter_next(&dict);
		}
	}
	return {ret_perc, ret_status};
}
