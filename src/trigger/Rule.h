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

#ifndef _CONTEXT_TRIGGER_RULE_H_
#define _CONTEXT_TRIGGER_RULE_H_

#include <string>
#include <Json.h>
#include "IContextListener.h"

namespace ctx {

namespace trigger {

	class RuleManager;

	class Rule : public IContextListener {
		private:
			struct ContextItem {
				std::string name;
				ctx::Json option;
				ContextItem(ctx::Json item) {
					std::string n;
					item.get(NULL, CT_RULE_EVENT_ITEM, &n);
					name = n;

					ctx::Json o;
					if (item.get(NULL, CT_RULE_EVENT_OPTION, &o))
						option = o.str();
				}
			};

			ctx::Json __statement;
			ContextItem* __event;
			std::list<ContextItem*> __condition;
			ctx::Json __action;
			ctx::Json __result;

			static RuleManager* __ruleMgr;

			void __clearResult(void);
			bool __setConditionOptionBasedOnEvent(ctx::Json& option);
			void __onContextDataPrepared(void);

			static gboolean __handleUninstalledRule(gpointer data);

		public:
			int id;
			std::string pkgId;

			Rule(int i, ctx::Json& d, const char* p, RuleManager* rm);
			~Rule();

			int start(void);
			int stop(void);

			void onEventReceived(std::string name, ctx::Json option, ctx::Json data);
			void onConditionReceived(std::string name, ctx::Json option, ctx::Json data);

	};

}	/* namespace trigger */
}	/* namespace ctx */

#endif	/* End of _CONTEXT_TRIGGER_RULE_H_ */
