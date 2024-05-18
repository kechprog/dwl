#pragma once

#include <cstdint>
#include <dbus-1.0/dbus/dbus.h>
#include <list>
#include <string>

enum class BatteryType {
	Regular,
	Pen,
	Headphones,
};

enum class BatteryStatus {
	Discharging,
	Charging,
};

class BatteryDevice {
public:
	BatteryDevice(const char *path, BatteryType type, DBusConnection *conn);
	void dbg_print() const;
	// ~BatteryDevice();
	bool operator==(const char *device_path) const;
	void operator()(DBusMessage *msg);

private:

	std::string device_path;
	DBusConnection *conn;

	BatteryType device_type;
	BatteryStatus status;
	int64_t time_till; /* either till fully charged or till empty */
	double percentage; /* till 2 decimal places */
};

class DbusListener {
public:
	DbusListener(void);
	~DbusListener();
	int get_fd(void) const;
	/* to be called on any poll event */
	void operator()(short int revents);

private:
	void on_add(DBusMessage *msg);
	void on_remove(DBusMessage *msg);
	std::list<BatteryDevice> devices;
	int dbus_fd;
	DBusConnection *conn;
};
