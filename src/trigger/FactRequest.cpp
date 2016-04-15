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

#include <Types.h>
#include "FactRequest.h"

using namespace ctx;
using namespace ctx::trigger;

FactRequest::FactRequest(int type, int reqId, const char* subj, const char* desc, ContextMonitor* ctxMonitor) :
	RequestInfo(type, reqId, subj, desc),
	__ctxMonitor(ctxMonitor),
	__replied(false)
{
}

FactRequest::~FactRequest()
{
	reply(ERR_OPERATION_FAILED);
}

const char* FactRequest::getClient()
{
	return "TRIGGER";
}

bool FactRequest::reply(int error)
{
	IF_FAIL_RETURN(!__replied && __ctxMonitor, true);
	__ctxMonitor->replyResult(__reqId, error);
	__replied = (error != ERR_NONE);
	return true;
}

bool FactRequest::reply(int error, Json& requestResult)
{
	IF_FAIL_RETURN(!__replied && __ctxMonitor, true);
	__ctxMonitor->replyResult(__reqId, error, &requestResult);
	__replied = (error != ERR_NONE);
	return true;
}

bool FactRequest::reply(int error, Json& requestResult, Json& dataRead)
{
	IF_FAIL_RETURN(!__replied && __ctxMonitor, true);
	__ctxMonitor->replyResult(__reqId, error, __subject.c_str(), &getDescription(), &dataRead);
	return (__replied = true);
}

bool FactRequest::publish(int error, Json& data)
{
	IF_FAIL_RETURN(__ctxMonitor, true);
	__ctxMonitor->publishFact(__reqId, error, __subject.c_str(), &getDescription(), &data);
	return true;
}
