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

#ifndef __CONTEXT_CONTEXT_TRIGGER_H__
#define __CONTEXT_CONTEXT_TRIGGER_H__

#include <event_driven.h>
#include "../request.h"
#include "fact.h"

namespace ctx {

	class fact_reader;
	class rule_manager;
	class client_request;
	class context_manager_impl;
	class context_trigger : public event_driven_thread {
		public:
			context_trigger();
			~context_trigger();

			bool init(ctx::context_manager_impl* mgr);
			void release();

			bool assign_request(ctx::request_info* request);
			void push_fact(int req_id, int error, const char* subject, ctx::json& option, ctx::json& data);

		private:
			enum event_type_e {
				ETYPE_REQUEST = 1,	// A request received from a client
				ETYPE_FACT,			// A context fact received from a CA
				ETYPE_INITIALIZE,	// Initialization
			};

			ctx::fact_reader *_reader;

			void on_thread_event_popped(int type, void* data);
			void delete_thread_event(int type, void* data);

			void process_request(ctx::request_info* request);
			void process_fact(ctx::context_fact* fact);
			void process_initialize(void);

			void add_rule(ctx::request_info* request);
			void remove_rule(ctx::request_info* request);
			void enable_rule(ctx::request_info* request);
			void disable_rule(ctx::request_info* request);
			void get_rule_by_id(ctx::request_info* request);
			void get_rule_ids(ctx::request_info* request);

			ctx::rule_manager* rule_mgr;
	};

}	/* namespace ctx */

#endif	/* End of __CONTEXT_CONTEXT_TRIGGER_H__ */
