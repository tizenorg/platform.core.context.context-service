/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <signal.h>
#include <app_manager.h>

#include <types_internal.h>
#include "server.h"
#include "client_request.h"
#include "access_control/peer_creds.h"
#include "DBusServer.h"

using namespace ctx;

static const gchar __introspection_xml[] =
	"<node>"
	"	<interface name='" DBUS_IFACE "'>"
	"		<method name='" METHOD_REQUEST "'>"
	"			<arg type='i' name='" ARG_REQTYPE "' direction='in'/>"
	"			<arg type='s' name='" ARG_COOKIE "' direction='in'/>"
	"			<arg type='i' name='" ARG_REQID "' direction='in'/>"
	"			<arg type='s' name='" ARG_SUBJECT "' direction='in'/>"
	"			<arg type='s' name='" ARG_INPUT "' direction='in'/>"
	"			<arg type='i' name='" ARG_RESULT_ERR "' direction='out'/>"
	"			<arg type='s' name='" ARG_RESULT_ADD "' direction='out'/>"
	"			<arg type='s' name='" ARG_OUTPUT "' direction='out'/>"
	"		</method>"
	"	</interface>"
	"</node>";

DBusServer *DBusServer::__theInstance = NULL;

DBusServer::DBusServer() :
	__owner(-1),
	__connection(NULL),
	__nodeInfo(NULL)
{
}

DBusServer::~DBusServer()
{
	__release();
}

void DBusServer::__processRequest(const char *sender, GVariant *param, GDBusMethodInvocation *invocation)
{
	gint reqType = 0;
	const gchar *cookie = NULL;
	gint reqId = 0;
	const gchar *subject = NULL;
	const gchar *input = NULL;

	g_variant_get(param, "(i&si&s&s)", &reqType, &cookie, &reqId, &subject, &input);
	IF_FAIL_VOID_TAG(reqType > 0 && reqId > 0 && cookie && subject && input, _E, "Invalid request");

	_I("[%d] ReqId: %d, Subject: %s", reqType, reqId, subject);
	_SI("Input: %s", input);

	credentials *creds = NULL;

	if (!peer_creds::get(__connection, sender, &creds)) {
		_E("Peer credentialing failed");
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(iss)", ERR_OPERATION_FAILED, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));
		return;
	}

	client_request *request = new(std::nothrow) client_request(reqType, reqId, subject, input, creds, sender, invocation);
	if (!request) {
		_E("Memory allocation failed");
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(iss)", ERR_OPERATION_FAILED, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));
		delete creds;
		return;
	}

	server::send_request(request);
}

void DBusServer::__onRequestReceived(GDBusConnection *conn, const gchar *sender,
		const gchar *path, const gchar *iface, const gchar *name,
		GVariant *param, GDBusMethodInvocation *invocation, gpointer userData)
{
	IF_FAIL_VOID_TAG(STR_EQ(path, DBUS_PATH), _W, "Invalid path: %s", path);
	IF_FAIL_VOID_TAG(STR_EQ(iface, DBUS_IFACE), _W, "Invalid interface: %s", path);

	if (STR_EQ(name, METHOD_REQUEST)) {
		__theInstance->__processRequest(sender, param, invocation);
	} else {
		_W("Invalid method: %s", name);
	}
}

void DBusServer::__onBusAcquired(GDBusConnection *conn, const gchar *name, gpointer userData)
{
	GDBusInterfaceVTable vtable;
	vtable.method_call = __onRequestReceived;
	vtable.get_property = NULL;
	vtable.set_property = NULL;

	guint regId = g_dbus_connection_register_object(conn, DBUS_PATH,
			__theInstance->__nodeInfo->interfaces[0], &vtable, NULL, NULL, NULL);

	if (regId <= 0) {
		_E("Failed to acquire dbus");
		raise(SIGTERM);
	}

	__theInstance->__connection = conn;
	_I("Dbus connection acquired");
}

void DBusServer::__onNameAcquired(GDBusConnection *conn, const gchar *name, gpointer userData)
{
	_SI("Dbus name acquired: %s", name);
	server::activate();
}

void DBusServer::__onNameLost(GDBusConnection *conn, const gchar *name, gpointer userData)
{
	_E("Dbus name lost");
	raise(SIGTERM);
}

void DBusServer::__onCallDone(GObject *source, GAsyncResult *res, gpointer userData)
{
	_I("Call %u done", *static_cast<unsigned int*>(userData));

	GDBusConnection *conn = G_DBUS_CONNECTION(source);
	GError *error = NULL;
	g_dbus_connection_call_finish(conn, res, &error);
	HANDLE_GERROR(error);
}

bool DBusServer::__init()
{
	__nodeInfo = g_dbus_node_info_new_for_xml(__introspection_xml, NULL);
	IF_FAIL_RETURN_TAG(__nodeInfo != NULL, false, _E, "Initialization failed");

	__owner = g_bus_own_name(G_BUS_TYPE_SESSION, DBUS_DEST, G_BUS_NAME_OWNER_FLAGS_NONE,
			__onBusAcquired, __onNameAcquired, __onNameLost, NULL, NULL);

	__theInstance = this;
	return true;
}

void DBusServer::__release()
{
	if (__connection) {
		g_dbus_connection_flush_sync(__connection, NULL, NULL);
	}

	if (__owner > 0) {
		g_bus_unown_name(__owner);
		__owner = 0;
	}

	if (__connection) {
		g_dbus_connection_close_sync(__connection, NULL, NULL);
		g_object_unref(__connection);
		__connection = NULL;
	}

	if (__nodeInfo) {
		g_dbus_node_info_unref(__nodeInfo);
		__nodeInfo = NULL;
	}
}

void DBusServer::__publish(const char *dest, int reqId, const char *subject, int error, const char *data)
{
	_SI("Publish: %s, %d, %s, %#x, %s", dest, reqId, subject, error, data);

	GVariant *param = g_variant_new("(isis)", reqId, subject, error, data);
	IF_FAIL_VOID_TAG(param, _E, "Memory allocation failed");

	g_dbus_connection_call(__connection, dest, DBUS_PATH, DBUS_IFACE,
			METHOD_RESPOND, param, NULL, G_DBUS_CALL_FLAGS_NONE, DBUS_TIMEOUT, NULL, NULL, NULL);
}

void DBusServer::__call(const char *dest, const char *obj, const char *iface, const char *method, GVariant *param)
{
	static unsigned int callCount = 0;
	++callCount;

	_SI("Call %u: %s, %s, %s.%s", callCount, dest, obj, iface, method);

	g_dbus_connection_call(__connection, dest, obj, iface, method, param, NULL,
			G_DBUS_CALL_FLAGS_NONE, DBUS_TIMEOUT, NULL, __onCallDone, &callCount);
}

void DBusServer::publish(std::string dest, int reqId, std::string subject, int error, std::string data)
{
	IF_FAIL_VOID_TAG(__theInstance, _E, "Not initialized");
	__theInstance->__publish(dest.c_str(), reqId, subject.c_str(), error, data.c_str());
}

void DBusServer::call(std::string dest, std::string obj, std::string iface, std::string method, GVariant *param)
{
	IF_FAIL_VOID_TAG(__theInstance, _E, "Not initialized");
	__theInstance->__call(dest.c_str(), obj.c_str(), iface.c_str(), method.c_str(), param);
}
