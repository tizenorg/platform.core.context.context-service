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

#ifndef _CONTEXT_PROVIDER_HANDLER_H_
#define _CONTEXT_PROVIDER_HANDLER_H_

#include <string>
#include <list>
#include <map>
#include <ContextProvider.h>
#include "ProviderLoader.h"

namespace ctx {

	class Credentials;
	class RequestInfo;

	class ProviderHandler {
		typedef std::list<RequestInfo*> RequestList;
		typedef std::map<std::string, ProviderHandler*> InstanceMap;

	public:
		bool isSupported();
		bool isAllowed(const Credentials *creds);

		void subscribe(RequestInfo *request);
		void unsubscribe(RequestInfo *request);
		void read(RequestInfo *request);
		void write(RequestInfo *request);

		bool publish(ctx::Json &option, int error, ctx::Json &dataUpdated);
		bool replyToRead(ctx::Json &option, int error, ctx::Json &dataRead);

		static ProviderHandler* getInstance(const char *subject, bool force);
		static void purge();

	private:
		const char *__subject;
		ContextProvider *__provider;
		RequestList __subscribeRequests;
		RequestList __readRequests;
		ProviderLoader __loader;

		static InstanceMap __instanceMap;

		ProviderHandler(const char *subject);
		~ProviderHandler();

		bool __loadProvider();
		RequestList::iterator __findRequest(RequestList &reqList, Json &option);
		RequestList::iterator __findRequest(RequestList &reqList, std::string client, int reqId);
		RequestList::iterator __findRequest(RequestList::iterator begin, RequestList::iterator end, Json &option);

	};	/* class ProviderHandler */

}	/* namespace ctx */

#endif	/* _CONTEXT_PROVIDER_HANDLER_H_ */
