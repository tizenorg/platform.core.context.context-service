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

#include <Json.h>

namespace ctx {

	class context_manager_impl;
	class rule_manager;
	class template_manager {
	public:
		static template_manager* get_instance();
		static void set_manager(ctx::context_manager_impl* ctx_mgr, ctx::rule_manager* rule_mgr);
		static void destroy();

		bool init();
		void apply_templates();
		int get_template(std::string &subject, ctx::Json* tmpl);

	private:
		template_manager();
		template_manager(const template_manager& other);
		~template_manager();

		static template_manager *_instance;
		static context_manager_impl *_context_mgr;
		static rule_manager *_rule_mgr;

		std::string add_template(std::string &subject, int &operation, ctx::Json &attributes, ctx::Json &options, std::string &owner);
		std::string remove_template(std::string &subject);

	};	/* class template_manager */

}	/* namespace ctx */

#endif	/* End of __TEMPLATE_MANAGER_H__ */
