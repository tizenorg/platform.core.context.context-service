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

#ifndef __CONTEXT_DBUS_SERVER_IMPL_H__
#define __CONTEXT_DBUS_SERVER_IMPL_H__

#include <sys/types.h>
#include <string>
#include <dbus_server_iface.h>

namespace ctx {
	class dbus_server_impl : public dbus_server_iface {
	public:
		dbus_server_impl();
		~dbus_server_impl();

		bool init();
		void release();

		void publish(const char *dest, int req_id, const char *subject, int error, const char *data);
		void call(const char *dest, const char *obj, const char *iface, const char *method, GVariant *param);
		int64_t subscribe_system_signal(const char *sender, const char *path, const char *iface, const char *name, dbus_listener_iface *listener);
		int64_t subscribe_session_signal(const char *sender, const char *path, const char *iface, const char *name, dbus_listener_iface *listener);
		void unsubscribe_system_signal(int64_t subscription_id);
		void unsubscribe_session_signal(int64_t subscription_id);

	private:
		bool connect_system();

	};	/* class ctx::dbus_server */

	namespace dbus_server {
		void publish(const char *dest, int req_id, const char *subject, int error, const char *data);
		void call(const char *dest, const char *obj, const char *iface, const char *method, GVariant *param);
	}
}	/* namespace ctx */

#endif	/* End of __CONTEXT_DBUS_SERVER_IMPL_H__ */
