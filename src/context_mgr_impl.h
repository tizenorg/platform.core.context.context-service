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

#ifndef __CONTEXT_MANAGER_IMPL_H__
#define __CONTEXT_MANAGER_IMPL_H__

#include <vector>
#include <list>
#include <map>
#include <context_mgr_iface.h>
#include "request.h"

#define TRIGGER_CLIENT_NAME "TRIGGER"

namespace ctx {

	class context_manager_impl : public context_manager_iface {
	public:
		typedef std::list<request_info*> request_list_t;

		context_manager_impl();
		~context_manager_impl();

		bool init();
		void release();

		void assign_request(ctx::request_info *request);
		bool is_supported(const char *subject);
		bool is_allowed(const char *client, const char *subject);

		/* From the interface class */
		bool register_provider(const char *subject, context_provider_info &provider_info);
		bool publish(const char *subject, ctx::json &option, int error, ctx::json &data_updated);
		bool reply_to_read(const char *subject, ctx::json &option, int error, ctx::json &data_read);

	private:
		typedef std::map<std::string, context_provider_info> provider_map_t;

		request_list_t subscribe_request_list;
		request_list_t read_request_list;
		provider_map_t provider_map;

		void subscribe(request_info *request);
		void unsubscribe(request_info *request);
		void read(request_info *request);
		void write(request_info *request);
		void is_supported(request_info *request);

		context_provider_iface *get_provider(request_info *request);

		static gboolean thread_switcher(gpointer data);
		bool _publish(const char *subject, ctx::json option, int error, ctx::json data_updated);
		bool _reply_to_read(const char *subject, ctx::json option, int error, ctx::json data_read);

		request_list_t::iterator find_request(request_list_t& r_list, std::string subject, json& option);
		request_list_t::iterator find_request(request_list_t& r_list, std::string client, int req_id);
		request_list_t::iterator find_request(request_list_t::iterator begin, request_list_t::iterator end, std::string subject, json& option);

	};	/* class context_manager_impl */

}	/* namespace ctx */

#endif	/* End of __CONTEXT_MANAGER_IMPL_H__ */
