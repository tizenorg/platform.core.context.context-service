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

#ifndef __CONTEXT_MONITOR_H__
#define __CONTEXT_MONITOR_H__

#include <list>
#include <map>
#include <Json.h>

namespace ctx {

	class fact_request;
	class context_manager_impl;
	class context_listener_iface;

	class context_monitor {
	public:
		static context_monitor* get_instance();
		static void set_context_manager(ctx::context_manager_impl* ctx_mgr);
		static void destroy();

		int subscribe(int rule_id, std::string subject, ctx::Json option, context_listener_iface* listener);
		int unsubscribe(int rule_id, std::string subject, ctx::Json option, context_listener_iface* listener);
		int read(std::string subject, Json option, context_listener_iface* listener);
		bool is_supported(std::string subject);
		bool is_allowed(const char *client, const char *subject);

		void reply_result(int req_id, int error, Json *request_result = NULL);
		void reply_result(int req_id, int error, const char *subject, ctx::Json *option, ctx::Json *fact);
		void publish_fact(int req_id, int error, const char *subject, ctx::Json *option, ctx::Json *fact);

	private:
		context_monitor();
		context_monitor(const context_monitor& other);
		~context_monitor();

		static context_monitor *_instance;
		static context_manager_impl *_context_mgr;

		int _subscribe(const char* subject, ctx::Json* option, context_listener_iface* listener);
		void _unsubscribe(const char *subject, int subscription_id);
		int _read(const char *subject, ctx::Json *option, context_listener_iface* listener);

		typedef std::list<context_listener_iface*> listener_list_t;

		struct subscr_info_s {
			int sid;
			std::string subject;
			ctx::Json option;
			listener_list_t listener_list;

			subscr_info_s(int id, const char *subj, ctx::Json *opt)
				: sid(id), subject(subj)
			{
				if (opt)
					option = *opt;
			}
		};

		typedef std::map<int, subscr_info_s*> subscr_map_t;
		subscr_map_t subscr_map;
		subscr_map_t read_map;

		int find_sub(request_type type, const char *subject, ctx::Json *option);
		bool add_sub(request_type type, int sid, const char *subject, ctx::Json *option, context_listener_iface* listener);
		void remove_sub(request_type type, const char *subject, ctx::Json *option);
		void remove_sub(request_type type, int sid);
		int add_listener(request_type type, int sid, context_listener_iface* listener);
		int remove_listener(request_type type, int sid, context_listener_iface* listener);

	};	/* class context_monitor */

}	/* namespace ctx */

#endif	/* End of __CONTEXT_MONITOR_H__ */
