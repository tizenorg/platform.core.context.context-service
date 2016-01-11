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

#ifndef __TEMPLATE_MANAGER_H__
#define __TEMPLATE_MANAGER_H__

#include <json.h>

namespace ctx {

	class context_manager_impl;

	class template_manager {
	public:
		template_manager(ctx::context_manager_impl* ctx_mgr);
		~template_manager();

		void apply_templates(void);
		bool get_fact_definition(std::string &subject, int &operation, ctx::json &attributes, ctx::json &options);
		int get_template(std::string &subject, ctx::json* tmpl);

	private:
		context_manager_impl *_context_mgr;

	};	/* class template_manager */

}	/* namespace ctx */

#endif	/* End of __TEMPLATE_MANAGER_H__ */
