#include "dbus_handles.hpp"
#include "dbus/dbus-protocol.h"
#include "dbus/dbus-shared.h"
#include "dbus/dbus.h"
#include "src/State.hpp"
#include "src/common.hpp"
#include "src/config.hpp"
#include <algorithm>
#include <cassert>
#include <format>
#include <functional>
#include <iostream>
#include <optional>
#include <ostream>
#include <pstl/glue_algorithm_defs.h>
#include <sstream>
#include <sys/poll.h>
#include <utility>

inline static void dbus_error_assert(DBusError *err) {
    if (dbus_error_is_set(err)) {
		std::stringstream ss;
        ss << "Dbus Error: " << err->message << "\n";
		die(ss.str().c_str());
    }
}

static inline void dbus_iter_for_each(DBusMessageIter* iter, std::function<void(DBusMessageIter *)> func) {
    do {
        int type = dbus_message_iter_get_arg_type(iter);
        if (type == DBUS_TYPE_INVALID)
            break;

        func(iter);

    } while (dbus_message_iter_next(iter));
}

template<typename T>
static inline T dbus_arg_variant_read(DBusMessageIter *iter) {
	DBusMessageIter variant_iter;
	T val;
	dbus_message_iter_recurse(iter, &variant_iter);
	dbus_message_iter_get_basic(&variant_iter, &val);

	return val;
}



template<typename T>
static inline T dbus_get(DBusConnection *conn, const char *path, const char *property) {

	DBusError err;
	dbus_error_init(&err);

	auto *msg = dbus_message_new_method_call(
		"org.freedesktop.UPower",
		path,
		"org.freedesktop.DBus.Properties",
		"Get"
	);

	const auto iface_name = "org.freedesktop.UPower.Device";
	dbus_message_append_args(
		msg,
		DBUS_TYPE_STRING, &iface_name,
		DBUS_TYPE_STRING, &property,
		DBUS_TYPE_INVALID
	);

	DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
	dbus_error_assert(&err);
	DBusMessageIter iter;
	dbus_message_iter_init(reply, &iter);

	dbus_message_unref(msg);
	dbus_error_free(&err);

	return dbus_arg_variant_read<T>(&iter);
}

using VardictValue = std::variant<uint32_t, double, int64_t>;

static inline std::unordered_map<std::string, VardictValue> 
vardict_to_map(DBusMessageIter *iter) {
    std::unordered_map<std::string, VardictValue> result;
    DBusMessageIter dict_entry;

    dbus_message_iter_recurse(iter, &dict_entry);
    while (dbus_message_iter_get_arg_type(&dict_entry) != DBUS_TYPE_INVALID) {
        if (dbus_message_iter_get_arg_type(&dict_entry) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter entry_iter;
            dbus_message_iter_recurse(&dict_entry, &entry_iter);

            if (dbus_message_iter_get_arg_type(&entry_iter) == DBUS_TYPE_STRING) {
                char *key;
                dbus_message_iter_get_basic(&entry_iter, &key);
                dbus_message_iter_next(&entry_iter);

                DBusMessageIter value_iter;
                dbus_message_iter_recurse(&entry_iter, &value_iter);
                int value_type = dbus_message_iter_get_arg_type(&value_iter);

                switch (value_type) {
                    case DBUS_TYPE_UINT32: {
                        uint32_t int_val;
                        dbus_message_iter_get_basic(&value_iter, &int_val);
                        result[key] = int_val;
                        break;
                    }
                    case DBUS_TYPE_DOUBLE: {
                        double double_val;
                        dbus_message_iter_get_basic(&value_iter, &double_val);
                        result[key] = double_val;
                        break;
                    }
		case DBUS_TYPE_INT64: {
			int64_t val;	
			dbus_message_iter_get_basic(&value_iter, &val);
			result[key] = val;
		}
                    default:
			std::cerr << "Unknown type for VardictValue" << std::endl;
                        break;
                }
            }
        }
        dbus_message_iter_next(&dict_entry);
    }
    return result;
}

template<typename T>
static inline std::optional<T> 
vardict_get(const std::unordered_map<std::string, VardictValue> &vardict_map, const std::string &key) {
    auto it = vardict_map.find(key);
    if (it != vardict_map.end()) {
        return std::get<T>(it->second);
    } else {
        return std::nullopt;
    }
}

static inline std::optional<BatteryType> get_type_from_path(std::string_view path) {
	const auto &res = std::find_if(config::battery::batteries.begin(), config::battery::batteries.end(), [&path](auto e) {
		return path.compare(0, e.first.size(), e.first) == 0;
	});

	if (res == config::battery::batteries.end()) {
		return std::nullopt;
	} else {
		return std::make_optional(res->second);
	}
}

DbusListener::DbusListener() {
	DBusError err;
	dbus_error_init(&err);

    this->conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	dbus_error_assert(&err);
	assert(this->conn);
	dbus_connection_get_unix_fd(this->conn, &this->dbus_fd);
	
	DBusMessage *msg = dbus_message_new_method_call(
		"org.freedesktop.UPower",  /* bus_name, god knows what this is */ 
		"/org/freedesktop/UPower", /* object path */ 
		"org.freedesktop.UPower",  /* interfce */ 
		"EnumerateDevices" 	   /* method */
	);
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(this->conn, msg, -1, &err);
	dbus_error_assert(&err);
	assert(reply);

	DBusMessageIter iter;
	if (!dbus_message_iter_init(reply, &iter))
		die("Dbus iter init");

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		die("Dbus invalid reply from upower, in EnumerateDevices");

	DBusMessageIter sub_iter;
	dbus_message_iter_recurse(&iter, &sub_iter);
	dbus_iter_for_each(&sub_iter, [&](DBusMessageIter *sub_iter) {
		const auto type = dbus_message_iter_get_arg_type(sub_iter);
		const char *object_path;

		switch (type) {
			case DBUS_TYPE_OBJECT_PATH: {
				dbus_message_iter_get_basic(sub_iter, &object_path);
				const auto type = get_type_from_path(object_path);

				if (!type.has_value())
					return;

				this->devices.emplace_back(object_path, type.value(), this->conn);

				break;
			}

			default:
				die("Expected only object paths");
			break;
		}
	});

	dbus_bus_add_match(
		this->conn, 
		"type='signal',interface='org.freedesktop.UPower',member='DeviceAdded'",
		&err
	);
	dbus_error_assert(&err);

	dbus_bus_add_match(
		this->conn, 
		"type='signal',interface='org.freedesktop.UPower',member='DeviceRemoved'",
		&err
	);
	dbus_error_assert(&err);

	dbus_error_free(&err);
	dbus_message_unref(msg);
	dbus_message_unref(reply);

	state::render();
}

void BatteryDevice::dbg_print() const {
        std::cout << "Battery Info - Device Path: " << device_path 
                  << "\nPercentage: " << percentage 
                  << "\nStatus: " << (status == BatteryStatus::Discharging ? "Discharging" : "Charging") 
                  << "\nTime Until " << (status == BatteryStatus::Discharging ? "Empty: " : "Full: ") << time_till 
                  << std::endl;
}

BatteryDevice::BatteryDevice(const char *path, BatteryType type, DBusConnection *conn) 
	: device_path {path},
	  conn 		  {conn},
	  device_type {type}
{

	DBusError err;
	dbus_error_init(&err);

	this->percentage = dbus_get<double>(
		conn,
		this->device_path.c_str(),
		"Percentage"
	);

	this->status = dbus_get<uint32_t>(
		conn,
		this->device_path.c_str(),
		"State"
	) == 2 ? BatteryStatus::Discharging : BatteryStatus::Charging;

	this->time_till = dbus_get<int64_t>(
		conn,
		this->device_path.c_str(),
		this->status == BatteryStatus::Discharging ? "TimeToEmpty" : "TimeToFull"
	);

	dbus_bus_add_match(
		this->conn, 
		std::format("type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',path='{}'", this->device_path).c_str(), 
		&err
	);	
}

void BatteryDevice::operator()(DBusMessage *msg) {
	DBusMessageIter iter;
	if (!dbus_message_iter_init(msg, &iter))
		die("Dbus iter init");
	assert(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
	dbus_message_iter_next(&iter);

	auto changed_props = vardict_to_map(&iter);
	
	auto percentage = vardict_get<double>(changed_props, "Percentage");
	if (percentage.has_value())
		this->percentage = percentage.value();

	auto status = vardict_get<uint32_t>(changed_props, "State");
	if (status.has_value()) {
		this->status = status.value() == 2 /* same as discharging */
			? BatteryStatus::Discharging
			: BatteryStatus::Charging;

		const auto time_till_prop_name = this->status == BatteryStatus::Discharging 
			? "TimeTillEmpty"
			: "TimeToFull";

		this->time_till = vardict_get<int64_t>(changed_props, time_till_prop_name)
			.value_or(0);
	}
}

const BatteryType BatteryDevice::get_type() const {
	return this->device_type;
}

const std::pair<BatteryStatus, int64_t> BatteryDevice::get_status() const {
	return std::make_pair(this->status, this->time_till);
}

const double BatteryDevice::get_percentage() const {
	return this->percentage;
}

bool BatteryDevice::operator==(const char *device_path) const {
	return this->device_path == device_path;
}

DbusListener::~DbusListener() {
	dbus_connection_unref(this->conn);
}

int DbusListener::get_fd(void) const {
	return this->dbus_fd;
}

/* to be called on any poll event */
void DbusListener::operator()(short int revents) {
	if (!(revents & POLLIN))
		return;

	if (!dbus_connection_read_write(this->conn, 0))
		die("Dbus connection read write");

	DBusMessage *msg;
	while ((msg = dbus_connection_pop_message(this->conn))) {
		if (dbus_message_is_signal(msg, "org.freedesktop.UPower", "DeviceAdded"))
			this->on_add(msg);
		else if (dbus_message_is_signal(msg, "org.freedesktop.UPower", "DeviceRemoved"))
			this->on_remove(msg);
		else if (dbus_message_is_signal(msg, "org.freedesktop.DBus.Properties", "PropertiesChanged"))
			for (auto &dev : this->devices) {
				const char *device_path = dbus_message_get_path(msg);
				if (dev != device_path)
					continue;
				
				dev(msg);
			}
		else
			std::cerr << "Unhandled message" << std::endl;

		dbus_message_unref(msg);
	}

	state::render();
}


const std::vector<BatteryDevice>& DbusListener::get_bat_devs() const {
	return this->devices;
}

void DbusListener::on_add(DBusMessage *msg) {
	DBusMessageIter iter;
	const char *obj_path;
	if (!dbus_message_iter_init(msg, &iter))
		die("Dbus iter init");

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_OBJECT_PATH)
		die("on add invalid argument");

	dbus_message_iter_get_basic(&iter, &obj_path);
	const auto type = get_type_from_path(obj_path);

	if (!type.has_value())
		return;

	this->devices.emplace_back(obj_path, type.value(), this->conn);
}


void DbusListener::on_remove(DBusMessage *msg) {
	DBusMessageIter iter;
	const char *obj_path;
	if (!dbus_message_iter_init(msg, &iter))
		die("Dbus iter init");

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_OBJECT_PATH)
		die("on add invalid argument");

	dbus_message_iter_get_basic(&iter, &obj_path);

	/* c++ way of removing the device that has specific path if any such exists */
	auto it = std::find_if(this->devices.begin(), this->devices.end(), [&obj_path](const auto &device) {
	    return device == obj_path;
	});

	if (it != this->devices.end()) {
	    this->devices.erase(it);
	}
}
