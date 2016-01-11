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
#include <json.h>
#include "../context_mgr_impl.h"
#include "fact.h"
#include "timer.h"

namespace ctx {

	class json;
	class context_fact;
	class fact_request;

	class context_monitor {
	public:
		context_monitor();
		~context_monitor();
		bool init(ctx::context_manager_impl* ctx_mgr, ctx::context_trigger* tr);

		int subscribe(int rule_id, std::string subject, ctx::json event);
		int unsubscribe(int rule_id, std::string subject, ctx::json option);
		int read(std::string subject, json option, ctx::json* result);
		bool is_supported(std::string subject);
		bool is_allowed(const char *client, const char *subject);

		void reply_result(int req_id, int error, json *request_result = NULL, json *fact = NULL);
		void publish_fact(int req_id, int error, const char *subject, json *option, json *fact);

		bool get_fact_definition(std::string &subject, int &operation, ctx::json &attributes, ctx::json &options);

	private:
		int _subscribe(const char* subject, json* option, bool wait_response);
		void _unsubscribe(const char *subject, int subscription_id);
		bool _read(const char *subject, json *option, context_fact& fact);

		ctx::context_trigger* _trigger;
		ctx::trigger_timer* timer;
		static context_manager_impl *_context_mgr;

		struct subscr_info_s {
			int sid;
			int cnt;
			std::string subject;
			ctx::json option;
			subscr_info_s(int id, const char *subj, ctx::json *opt)
				: sid(id), cnt(1), subject(subj)
			{
				if (opt)
					option = *opt;
			}
		};

		typedef std::map<int, subscr_info_s*> subscr_map_t;
		subscr_map_t subscr_map;

		int find_sub(const char *subject, json *option);
		bool add_sub(int sid, const char *subject, json *option);
		void remove_sub(const char *subject, json *option);
		void remove_sub(int sid);
		int increase_sub(int sid);
		int decrease_sub(int sid);

		static bool send_request(fact_request* req);
	};	/* class context_monitor */

}	/* namespace ctx */

#endif	/* End of __CONTEXT_MONITOR_H__ */
