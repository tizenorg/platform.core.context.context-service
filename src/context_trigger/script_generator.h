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

#ifndef __CONTEXT_SCRIPT_GENERATOR_H__
#define __CONTEXT_SCRIPT_GENERATOR_H__

namespace ctx {

	namespace script_generator {

		std::string generate_deftemplate(ctx::json* item);
		std::string generate_defclass(ctx::json* item);
		std::string generate_makeinstance(ctx::json* item);
		std::string generate_undefrule(std::string rule_id);
		std::string generate_defrule(std::string rule_id, ctx::json event_template, ctx::json rule, ctx::json* inst_names);
		std::string generate_fact(std::string item_name, ctx::json event_template, ctx::json option, ctx::json data);
		std::string generate_modifyinstance(std::string instance_name, ctx::json condition_template, ctx::json data);

	}

}	/* namespace ctx */

#endif	/* End of __CONTEXT_SCRIPT_GENERATOR_H__ */
