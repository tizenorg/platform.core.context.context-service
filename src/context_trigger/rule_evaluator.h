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

#ifndef __CONTEXT_RULE_EVALUATOR_H__
#define __CONTEXT_RULE_EVALUATOR_H__

namespace ctx {

	class Json;

	class rule_evaluator {
	private:
		rule_evaluator();
		bool evaluate_item(ctx::Json& rule_item, ctx::Json& fact_item);
		bool compare_int(std::string operation, int rule_var, int fact_var);
		bool compare_string(std::string operation, std::string rule_var, std::string fact_var);
		bool check_rule_event(ctx::Json& rule, ctx::Json& fact);
		ctx::Json find_condition_fact(ctx::Json& rule_condition, ctx::Json& fact);
		bool check_rule_condition(ctx::Json& rule, ctx::Json& fact);
		bool replace_data_references(ctx::Json& rule_data_arr, ctx::Json event_fact_data);
		bool replace_option_references(ctx::Json& rule_option, ctx::Json event_fact_data);
		bool replace_event_references(ctx::Json& rule, ctx::Json& fact);
		bool evaluate_data_string(ctx::Json& rule_data_arr, std::string fact_value_str);
		bool evaluate_data_int(ctx::Json& rule_data_arr, int fact_value_int);
	public:
		static bool evaluate_rule(ctx::Json rule, ctx::Json data);
	};

}	/* namespace ctx */

#endif	/* End of __CONTEXT_RULE_EVALUATOR_H__ */
