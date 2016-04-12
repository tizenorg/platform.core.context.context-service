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

#ifndef _CONTEXT_DBUS_SERVER_H_
#define _CONTEXT_DBUS_SERVER_H_

#include <glib.h>
#include <gio/gio.h>
#include <string>

namespace ctx {

	class DBusServer {
	public:
		~DBusServer();

		static void publish(std::string dest, int reqId, std::string subject, int error, std::string data);
		static void call(std::string dest, std::string obj, std::string iface, std::string method, GVariant *param);

	private:
		DBusServer();

		static void __onRequestReceived(GDBusConnection *conn, const gchar *sender,
				const gchar *path, const gchar *iface, const gchar *name,
				GVariant *param, GDBusMethodInvocation *invocation, gpointer user_data);
		static void __onBusAcquired(GDBusConnection *conn, const gchar *name, gpointer userData);
		static void __onNameAcquired(GDBusConnection *conn, const gchar *name, gpointer userData);
		static void __onNameLost(GDBusConnection *conn, const gchar *name, gpointer userData);
		static void __onCallDone(GObject *source, GAsyncResult *res, gpointer userData);

		bool __init();
		void __release();
		void __publish(const char *dest, int reqId, const char *subject, int error, const char *data);
		void __call(const char *dest, const char *obj, const char *iface, const char *method, GVariant *param);

		void __processRequest(const char *sender, GVariant *param, GDBusMethodInvocation *invocation);

		static DBusServer *__theInstance;

		guint __owner;
		GDBusConnection *__connection;
		GDBusNodeInfo *__nodeInfo;

		friend class Server;

	};	/* class ctx::DBusServer */

}	/* namespace ctx */

#endif	/* End of _CONTEXT_DBUS_SERVER_H_ */
