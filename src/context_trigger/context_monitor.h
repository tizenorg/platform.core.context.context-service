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

#include <map>
#include "timer.h"

namespace ctx {

	class json;
	class fact_reader;
	class context_fact;

	class context_monitor {
	public:
		context_monitor();
		~context_monitor();
		bool init(ctx::fact_reader* fr, ctx::context_trigger* tr);

		int subscribe(int rule_id, std::string subject, ctx::json event);
		int unsubscribe(int rule_id, std::string subject, ctx::json option);
		int read(std::string subject, json option, ctx::json* result);
		bool is_supported(std::string subject);
		bool is_allowed(const char *client, const char *subject);

	private:
		std::map<int, int> request_map;	// <rule_id, fact_read_req_id>
		std::map<int, int> read_req_cnt_map;	// <fact_read_req_id, count>
		ctx::context_trigger* trigger;
		ctx::trigger_timer* timer;

	};	/* class context_monitor */

}	/* namespace ctx */

#endif	/* End of __CONTEXT_MONITOR_H__ */
