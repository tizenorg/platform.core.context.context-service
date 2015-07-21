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

#ifndef __CLIPS_HANDLER_H__
#define __CLIPS_HANDLER_H__

namespace ctx {

	class json;
	class rule_manager;

	class clips_handler {
		public:
			clips_handler(ctx::rule_manager* rm);
			~clips_handler();

			int define_template(std::string& script);
			int define_class(std::string& script);
			int define_rule(std::string& script);
			int run_environment();
			int add_fact(std::string& fact);
			int route_string_command(std::string& fact);
			int make_instance(std::string& script);
			int unmake_instance(std::string& instance_name);
			static int execute_action(void);
			bool find_instance(std::string& instance_name);
			bool define_global_variable_string(std::string variable_name, std::string value);
			bool set_global_variable_string(std::string variable_name, std::string value);

		private:
			clips_handler();
			int init_environment(void);
			int env_build(std::string& script);
			void* find_instance_internal(std::string& instance_name);

   };	/* class clips_handler */

}	/* namespace ctx */

#endif	/* End of __CLIPS_HANDLER_H__ */
