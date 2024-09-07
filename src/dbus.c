#include <assert.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <dbus/dbus.h>
#include "dwl.h"

int dbus_event_handler(int fd, uint32_t mask, void *data);
inline static void dbus_error_assert(DBusError *err);

static DBusConnection *conn = NULL;
static int dbus_fd = -1;

inline static void dbus_error_assert(DBusError *err) {
    if (dbus_error_is_set(err)) {
       die("Dbus Error: %s\n",err->message);
    }
}

static bool has_accelerometer(void) {
    DBusMessage *msg, *reply;
    DBusMessageIter args;
    DBusError err;
    bool has_accel = false;

    // Initialize the error
    dbus_error_init(&err);

    // Create a message to call the Get method
    msg = dbus_message_new_method_call("net.hadess.SensorProxy",  // bus name
                                       "/net/hadess/SensorProxy",  // object path
                                       "org.freedesktop.DBus.Properties",  // interface
                                       "Get");  // method name
    if (msg == NULL) {
        fprintf(stderr, "Message Null\n");
        return false;
    }

    // Append the arguments to the message
    const char *interface_name = "net.hadess.SensorProxy";
    const char *property_name = "HasAccelerometer";

    dbus_message_append_args(msg,
                             DBUS_TYPE_STRING, &interface_name,
                             DBUS_TYPE_STRING, &property_name,
                             DBUS_TYPE_INVALID);

    // Send the message and wait for a reply
    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    dbus_message_unref(msg);  // free the message object
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Error: %s\n", err.message);
        dbus_error_free(&err);
        return false;
    }
    if (reply == NULL) {
        fprintf(stderr, "Reply Null\n");
        return false;
    }

    // Read the reply
    if (dbus_message_iter_init(reply, &args)) {
        if (DBUS_TYPE_VARIANT == dbus_message_iter_get_arg_type(&args)) {
            DBusMessageIter sub_iter;
            dbus_message_iter_recurse(&args, &sub_iter);
            if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&sub_iter)) {
                dbus_message_iter_get_basic(&sub_iter, &has_accel);
            }
        }
    }

    // Clean up
    dbus_message_unref(reply);

    return has_accel;
}

void dbus_init(struct wl_event_loop *loop) {
	DBusError err;
	dbus_error_init(&err);

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	dbus_error_assert(&err);
	assert(conn);
	dbus_connection_get_unix_fd(conn, &dbus_fd);

	if (!has_accelerometer()) {
		return;
	}

	wl_event_loop_add_fd(
		loop, 
		dbus_fd, 
		WL_EVENT_READABLE, 
		dbus_event_handler, 
		/*data=*/ NULL
	);

	dbus_error_free(&err);
}

int dbus_event_handler(int fd, uint32_t mask, void *data) {
	assert(fd | WL_EVENT_READABLE);

	return 0;
}

void dbus_cleanup(void) {
	dbus_connection_unref(conn);
}
