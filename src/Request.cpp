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

#include <glib.h>
#include <Types.h>
#include "Request.h"

ctx::RequestInfo::RequestInfo(int type, int reqId, const char* subj, const char* desc) :
	__type(type),
	__reqId(reqId),
	__subject(subj),
	__description(desc)
{
}

ctx::RequestInfo::~RequestInfo()
{
}

int ctx::RequestInfo::getType()
{
	return __type;
}

int ctx::RequestInfo::getId()
{
	return __reqId;
}

const ctx::Credentials* ctx::RequestInfo::getCredentials()
{
	return NULL;
}

const char* ctx::RequestInfo::getPackageId()
{
	return NULL;
}

const char* ctx::RequestInfo::getClient()
{
	return NULL;
}

const char* ctx::RequestInfo::getSubject()
{
	return __subject.c_str();
}

ctx::Json& ctx::RequestInfo::getDescription()
{
	return __description;
}
