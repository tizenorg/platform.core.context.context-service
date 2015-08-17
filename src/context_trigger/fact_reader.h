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

#ifndef __CONTEXT_FACT_READER_H__
#define __CONTEXT_FACT_READER_H__

#include <list>
#include <json.h>
#include "../context_mgr_impl.h"
#include "fact.h"

namespace ctx {

	class context_trigger;

	class fact_reader {
	public:
		fact_reader(context_manager_impl *mgr, context_trigger *trigger);
		~fact_reader();

		bool is_supported(const char *subject);
		bool is_allowed(const char *client, const char *subject);
		bool get_fact_definition(std::string &subject, int &operation, ctx::json &attributes, ctx::json &options);

		int subscribe(const char *subject, json *option, bool wait_response = false);
		void unsubscribe(const char *subject, json *option);
		void unsubscribe(const char *subject, int subscription_id);
		bool read(const char *subject, json *option, context_fact& fact);

		void reply_result(int req_id, int error, json *request_result = NULL, json *fact = NULL);
		void publish_fact(int req_id, int error, const char *subject, json *option, json *fact);

	private:
		static context_manager_impl *_context_mgr;
		static context_trigger *_trigger;

		struct subscr_info_s {
			int sid;
			std::string subject;
			ctx::json option;
			subscr_info_s(int id, const char *subj, ctx::json *opt)
				: sid(id), subject(subj)
			{
				if (opt)
					option = *opt;
			}
		};

		typedef std::list<subscr_info_s*> subscr_list_t;
		subscr_list_t subscr_list;

		int find_sub(const char *subject, json *option);
		bool add_sub(int sid, const char *subject, json *option);
		void remove_sub(const char *subject, json *option);
		void remove_sub(int sid);

		static gboolean send_request(gpointer data);
	};

}	/* namespace ctx */

#endif	/* End of __CONTEXT_FACT_READER_H__ */
