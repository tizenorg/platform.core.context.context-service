/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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

#ifndef __CONTEXT_CONTEXT_TRIGGER_FACT_H__
#define __CONTEXT_CONTEXT_TRIGGER_FACT_H__

#include <string>
#include <json.h>

namespace ctx {

	class context_fact {
		private:
			int req_id;
			int error;
			std::string subject;
			ctx::json option;
			ctx::json data;
			std::string zone_name;

		public:
			context_fact();
			context_fact(int id, int err, const char* s, ctx::json& o, ctx::json& d, const char* z);
			~context_fact();

			void set_req_id(int id);
			void set_error(int err);
			void set_subject(const char* s);
			void set_option(ctx::json& o);
			void set_zone_name(const char* z);
			void set_data(ctx::json& d);

			int get_req_id();
			int get_error();
			const char* get_subject();
			const char* get_zone_name();
			ctx::json& get_option();
			ctx::json& get_data();
	};

}	/* namespace ctx */

#endif	/* End of __CONTEXT_CONTEXT_TRIGGER_FACT_H__ */
