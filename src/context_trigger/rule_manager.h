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

#ifndef __RULE_MANAGER_H__
#define __RULE_MANAGER_H__

#include <set>
#include <map>

namespace ctx {

	class json;
	class context_trigger;
	class trigger_rule;

	class rule_manager {
		public:
			rule_manager();
			~rule_manager();

			bool init();
			int add_rule(std::string creator, const char* app_id, ctx::json rule, ctx::json* rule_id);
			int remove_rule(int rule_id);
			int enable_rule(int rule_id);
			int disable_rule(int rule_id);
			int get_rule_by_id(std::string creator, int rule_id, ctx::json* request_result);
			int get_rule_ids(std::string creator, ctx::json* request_result);
			int check_rule(std::string creator, int rule_id);
			bool is_rule_enabled(int rule_id);
			int disable_rule_with_removed_item(std::string& subject);

			static bool is_uninstalled_package(std::string app_id);

		private:
			bool reenable_rule(void);
			int verify_rule(ctx::json& rule, const char* app_id);
			int64_t get_duplicated_rule_id(std::string creator, ctx::json& rule);
			bool rule_data_arr_elem_equals(ctx::json& lelem, ctx::json& relem);
			bool rule_item_equals(ctx::json& litem, ctx::json& ritem);
			bool rule_equals(ctx::json& lrule, ctx::json& rrule);
			int get_uninstalled_app(void);
			int clear_rule_of_uninstalled_app(bool is_init = false);

			std::set<std::string> uninstalled_apps;

			typedef std::map<int, trigger_rule*> rule_map_t;
			rule_map_t rule_map;
   };	/* class rule_manager */

}	/* namespace ctx */

#endif	/* End of __RULE_MANAGER_H__ */
