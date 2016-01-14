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

#ifndef __CONTEXT_TRIGGER_RULE_H__
#define __CONTEXT_TRIGGER_RULE_H__

#include <string>
#include <json.h>
#include "context_listener_iface.h"

namespace ctx {

	class rule_manager;

	class trigger_rule : public context_listener_iface {
		private:
			struct context_item_s {
				std::string name;
				ctx::json option;
				context_item_s(ctx::json item) {
					std::string n;
					item.get(NULL, CT_RULE_EVENT_ITEM, &n);
					name = n;

					ctx::json o;
					if (item.get(NULL, CT_RULE_EVENT_OPTION, &o))
						option = o.str();
				}
			};

			typedef struct context_item_s* context_item_t;
			ctx::json statement;
			context_item_t event;
			std::list<context_item_t> condition;
			ctx::json action;
			ctx::json result;

			static rule_manager* rule_mgr;

			void clear_result(void);
			bool set_condition_option_based_on_event(ctx::json& option);
			void on_context_data_prepared(void);

			static gboolean handle_uninstalled_rule(gpointer data);

		public:
			int id;
			std::string creator;

			trigger_rule(int i, ctx::json& d, const char* c, rule_manager* rm);
			~trigger_rule();

			int start(void);
			int stop(void);

			void on_event_received(std::string name, ctx::json option, ctx::json data);
			void on_condition_received(std::string name, ctx::json option, ctx::json data);

	};

}	/* namespace ctx */

#endif	/* End of __CONTEXT_TRIGGER_RULE_H__ */
