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

#ifndef _CONTEXT_POLICY_REQUEST_H_
#define _CONTEXT_POLICY_REQUEST_H_

#include "../Request.h"

namespace ctx {

	class PolicyRequest : public RequestInfo {
	public:
		PolicyRequest(int type, int reqId, const char *subj, const char *desc);
		~PolicyRequest();

		bool reply(int error);
		bool reply(int error, ctx::Json &requestResult);
		bool reply(int error, ctx::Json &requestResult, ctx::Json &dataRead);
		bool publish(int error, ctx::Json &data);
	};

}	/* namespace ctx */

#endif	/* End of _CONTEXT_POLICY_REQUEST_H_ */
