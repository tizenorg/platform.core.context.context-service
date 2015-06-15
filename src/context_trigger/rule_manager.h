/*
* context-service
*
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
*
*/

#ifndef __RULE_MANAGER_H__
#define __RULE_MANAGER_H__

#include "clips_handler.h"
#include "context_monitor.h"

namespace ctx {

	class json;
	class context_trigger;
	class fact_reader;

	class rule_manager {
		public:
			rule_manager();
			~rule_manager();
			bool init(ctx::context_trigger* tr, ctx::fact_reader* fr);
			int add_rule(std::string creator, ctx::json rule, std::string zone, ctx::json* rule_id);
			int remove_rule(int rule_id);
			int enable_rule(int rule_id);
			int disable_rule(int rule_id);
			int get_rule_by_id(std::string creator, int rule_id, ctx::json* request_result);
			int get_rule_ids(std::string creator, ctx::json* request_result);
			int check_rule(std::string creator, int rule_id);
			bool is_rule_enabled(int rule_id);

			void on_event_received(std::string item, ctx::json option, ctx::json data, std::string zone);
			void on_rule_triggered(int rule_id);

		private:
			clips_handler clips_h;
			context_monitor c_monitor;

			bool reenable_rule(void);
			int verify_rule(ctx::json& rule, const char* app_id, const char* zone);
			int64_t get_duplicated_rule(std::string creator, std::string zone, ctx::json& rule);
			bool rule_data_arr_elem_equals(ctx::json& lelem, ctx::json& relem);
			bool rule_item_equals(ctx::json& litem, ctx::json& ritem);
			bool rule_equals(ctx::json& lrule, ctx::json& rrule);
			std::string get_instance_name(std::string name, ctx::json& condition);
			void make_condition_option_based_on_event_data(ctx::json& ctemplate, ctx::json& edata, ctx::json* coption);

			std::map<std::string, int> cond_cnt_map; // <condition instance name, count>

   };	/* class rule_manager */

}	/* namespace ctx */

#endif	/* End of __RULE_MANAGER_H__ */
