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

#ifndef _CONTEXT_REQUEST_INFO_H_
#define _CONTEXT_REQUEST_INFO_H_

#include <string>
#include <Json.h>

namespace ctx {

	/* Forward declaration */
	class Credentials;

	class RequestInfo {
	public:
		RequestInfo(int type, int reqId, const char *subj, const char *desc);
		virtual ~RequestInfo();

		int getType();
		int getId();
		const char* getSubject();
		ctx::Json& getDescription();

		virtual const Credentials* getCredentials();
		virtual const char* getPackageId();
		/* TODO: remove this getClient() */
		virtual const char* getClient();
		virtual bool reply(int error) = 0;
		virtual bool reply(int error, ctx::Json &requestResult) = 0;
		virtual bool reply(int error, ctx::Json &requestResult, ctx::Json &dataRead) = 0;
		virtual bool publish(int error, ctx::Json &data) = 0;

	protected:
		int __type;
		int __reqId;
		std::string __subject;
		ctx::Json __description;
	};

}	/* namespace ctx */

#endif	/* End of _CONTEXT_REQUEST_INFO_H_ */
