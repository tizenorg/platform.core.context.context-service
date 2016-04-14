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

#ifndef _CONTEXT_TRIGGER_TEMPLATE_MANAGER_H_
#define _CONTEXT_TRIGGER_TEMPLATE_MANAGER_H_

#include <Json.h>
#include <DatabaseManager.h>

namespace ctx {

	class ContextManagerImpl;

namespace trigger {

	class RuleManager;

	class TemplateManager {
	public:
		static TemplateManager* getInstance();
		static void setManager(ContextManagerImpl* ctxMgr, RuleManager* ruleMgr);
		static void destroy();

		bool init();
		void applyTemplates();
		int getTemplate(std::string &subject, Json* tmpl);
		void registerTemplate(std::string subject, int operation, Json attributes, Json options, std::string owner);
		void unregisterTemplate(std::string subject);

	private:
		TemplateManager();
		TemplateManager(const TemplateManager& other);
		~TemplateManager();

		static TemplateManager *__instance;
		static ContextManagerImpl *__contextMgr;
		static RuleManager *__ruleMgr;

		DatabaseManager __dbManager;

		std::string __addTemplate(std::string &subject, int &operation, Json &attributes, Json &options, std::string &owner);
		std::string __removeTemplate(std::string &subject);

	};	/* class TemplateManager */

}	/* namespace trigger */
}	/* namespace ctx */

#endif	/* End of _CONTEXT_TRIGGER_TEMPLATE_MANAGER_H_ */
