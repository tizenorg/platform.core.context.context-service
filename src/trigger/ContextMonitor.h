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

#ifndef _CONTEXT_TRIGGER_CONTEXT_MONITOR_H_
#define _CONTEXT_TRIGGER_CONTEXT_MONITOR_H_

#include <list>
#include <map>
#include <Json.h>
#include <DBusTypes.h>

namespace ctx {

	class ContextManager;

namespace trigger {

	class IContextListener;
	class FactRequest;

	class ContextMonitor {
	public:
		static ContextMonitor* getInstance();
		static void setContextManager(ContextManager* ctx_mgr);
		static void destroy();

		int subscribe(int ruleId, std::string subject, Json option, IContextListener* listener);
		int unsubscribe(int ruleId, std::string subject, Json option, IContextListener* listener);
		int read(std::string subject, Json option, IContextListener* listener);
		bool isSupported(std::string subject);
		bool isAllowed(const char *client, const char *subject);

		void replyResult(int reqId, int error, Json *requestResult = NULL);
		void replyResult(int reqId, int error, const char *subject, Json *option, Json *fact);
		void publishFact(int reqId, int error, const char *subject, Json *option, Json *fact);

	private:
		ContextMonitor();
		ContextMonitor(const ContextMonitor& other);
		~ContextMonitor();

		static ContextMonitor *__instance;
		static ContextManager *__contextMgr;

		int __subscribe(const char* subject, Json* option, IContextListener* listener);
		void __unsubscribe(const char *subject, int subscriptionId);
		int __read(const char *subject, Json *option, IContextListener* listener);

		struct SubscrInfo {
			int sid;
			std::string subject;
			Json option;
			std::list<IContextListener*> listenerList;

			SubscrInfo(int id, const char *subj, Json *opt) :
				sid(id),
				subject(subj)
			{
				if (opt)
					option = *opt;
			}
		};

		std::map<int, SubscrInfo*> __subscrMap;
		std::map<int, SubscrInfo*> ___readMap;

		int __findSub(RequestType type, const char *subject, Json *option);
		bool __addSub(RequestType type, int sid, const char *subject, Json *option, IContextListener* listener);
		void __removeSub(RequestType type, const char *subject, Json *option);
		void __removeSub(RequestType type, int sid);
		int __addListener(RequestType type, int sid, IContextListener* listener);
		int __removeListener(RequestType type, int sid, IContextListener* listener);

	};	/* class ContextMonitor */

}	/* namespace trigger */
}	/* namespace ctx */

#endif	/* End of _CONTEXT_TRIGGER_CONTEXT_MONITOR_H_ */
