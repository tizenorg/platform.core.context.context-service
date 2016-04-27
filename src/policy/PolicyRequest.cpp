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

#include "PolicyRequest.h"

using namespace ctx;

PolicyRequest::PolicyRequest(int type, int reqId, const char *subj, const char *desc) :
	RequestInfo(type, reqId, subj, desc)
{
}

PolicyRequest::~PolicyRequest()
{
}

bool PolicyRequest::reply(int error)
{
	return true;
}

bool PolicyRequest::reply(int error, ctx::Json &requestResult)
{
	return true;
}

bool PolicyRequest::reply(int error, ctx::Json &requestResult, ctx::Json &dataRead)
{
	return true;
}

bool PolicyRequest::publish(int error, ctx::Json &data)
{
	return true;
}
