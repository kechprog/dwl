#pragma once

#include <cstdint>
#include <dbus-1.0/dbus/dbus.h>
#include <string>
#include <vector>

enum class BatteryType : int {
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
	bool operator==(const char *device_path) const;
	void operator()(DBusMessage *msg);

	const BatteryType get_type() const;
	const std::pair<BatteryStatus, int64_t> get_status() const;
	const double get_percentage() const;

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

	const std::vector<BatteryDevice>& get_bat_devs() const;

private:
	void on_add(DBusMessage *msg);
	void on_remove(DBusMessage *msg);
	std::vector<BatteryDevice> devices;
	int dbus_fd;
	DBusConnection *conn;
};
