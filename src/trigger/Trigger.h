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

#ifndef _CONTEXT_TRIGGER_TRIGGER_H_
#define _CONTEXT_TRIGGER_TRIGGER_H_

#include "../Request.h"

namespace ctx {

	class client_request;
	class ContextManager;

namespace trigger {

	class RuleManager;

	class Trigger {
		public:
			Trigger();
			~Trigger();

			bool init(ContextManager* ctxMgr);
			void release();

			bool assignRequest(RequestInfo* request);

		private:
			void __processRequest(RequestInfo* request);
			void __processInitialize(ContextManager* mgr);

			void __addRule(RequestInfo* request);
			void __removeRule(RequestInfo* request);
			void __enableRule(RequestInfo* request);
			void __disableRule(RequestInfo* request);
			void __getRuleById(RequestInfo* request);
			void __getRuleIds(RequestInfo* request);
			void __getTemplate(RequestInfo* request);

			RuleManager* __ruleMgr;
	};

}	/* namespace trigger */
}	/* namespace ctx */

#endif	/* End of _CONTEXT_TRIGGER_TRIGGER_H_ */
