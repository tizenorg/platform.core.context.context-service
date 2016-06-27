/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#ifndef _CONTEXT_POLICY_MANAGER_H_
#define _CONTEXT_POLICY_MANAGER_H_

#include <map>

namespace ctx {

	class ContextManger;

	class PolicyManager {
	public:
		~PolicyManager();

	private:
		PolicyManager(ContextManager *contextMgr);

		void __subscribe(const char *subject);

		ContextManager *__contextMgr;
		std::map<int, const char*> __subscriptionMap;

		friend class Server;
	};

}	/* namespace ctx */

#endif	/* _CONTEXT_POLICY_MANAGER_H_ */
