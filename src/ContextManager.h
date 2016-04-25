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

#ifndef _CONTEXT_CONTEXT_MANAGER_H_
#define _CONTEXT_CONTEXT_MANAGER_H_

#include <map>
#include <IContextManager.h>
#include "ProviderLoader.h"

namespace ctx {

	/* Forward declaration */
	class Credentials;
	class RequestInfo;
	class ProviderHandler;

	class ContextManager : public IContextManager {
	public:
		~ContextManager();

		bool init();
		void release();

		void assignRequest(ctx::RequestInfo *request);
		bool isSupported(const char *subject);
		bool isAllowed(const Credentials *creds, const char *subject);
		bool popTriggerItem(std::string &subject, int &operation, ctx::Json &attributes, ctx::Json &options, std::string &owner, bool& unregister);

		/* From the interface class */
		bool registerProvider(const char *subject, const char *privilege, ContextProvider *provider);
		bool unregisterProvider(const char *subject);
		bool registerTriggerItem(const char *subject, int operation, ctx::Json attributes, ctx::Json options, const char *owner = NULL);
		bool unregisterTriggerItem(const char *subject);
		bool publish(const char *subject, ctx::Json &option, int error, ctx::Json &dataUpdated);
		bool replyToRead(const char *subject, ctx::Json &option, int error, ctx::Json &dataRead);

	private:
		ContextManager();

		static gboolean __threadSwitcher(gpointer data);

		void __publish(const char *subject, ctx::Json &option, int error, ctx::Json &dataUpdated);
		void __replyToRead(const char *subject, ctx::Json &option, int error, ctx::Json &dataRead);

		/* For custom request */
		bool __handleCustomRequest(ctx::RequestInfo* request);

		bool __initialized;
		std::map<std::string, ProviderHandler*> __providerHandleMap;
		ProviderLoader __providerLoader;

		friend class Server;

	};	/* class ContextManager */

}	/* namespace ctx */

#endif	/* _CONTEXT_CONTEXT_MANAGER_H_ */
