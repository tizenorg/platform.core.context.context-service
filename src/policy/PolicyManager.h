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

namespace ctx {

	class ContextManger;

	class PolicyManager {
	public:
		~PolicyManager();

	private:
		PolicyManager(ContextManager *contextMgr);

		void __init();
		void __release();

		void __subscribe(const char *subject, int &reqId);
		void __unsubscribe(const char *subject, int reqId);

		ContextManager *__contextMgr;
		int __ridWifiState;
		int __ridAppLogging;
		int __ridMediaLogging;
		int __ridSocialLogging;
		int __ridPlaceDetection;

		friend class Server;
	};

}	/* namespace ctx */

#endif	/* End of _CONTEXT_POLICY_MANAGER_H_ */
