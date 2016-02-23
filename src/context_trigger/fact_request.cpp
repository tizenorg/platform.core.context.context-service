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

#include <types_internal.h>
#include "fact_request.h"

ctx::fact_request::fact_request(int type, int req_id, const char* subj, const char* desc, context_monitor* ctx_monitor)
	: request_info(type, req_id, subj, desc)
	, _ctx_monitor(ctx_monitor)
	, replied(false)
{
}

ctx::fact_request::~fact_request()
{
	reply(ERR_OPERATION_FAILED);
}

const char* ctx::fact_request::get_client()
{
	return "TRIGGER";
}

bool ctx::fact_request::reply(int error)
{
	IF_FAIL_RETURN(!replied && _ctx_monitor, true);
	_ctx_monitor->reply_result(_req_id, error);
	replied = (error != ERR_NONE);
	return true;
}

bool ctx::fact_request::reply(int error, ctx::Json& request_result)
{
	IF_FAIL_RETURN(!replied && _ctx_monitor, true);
	_ctx_monitor->reply_result(_req_id, error, &request_result);
	replied = (error != ERR_NONE);
	return true;
}

bool ctx::fact_request::reply(int error, ctx::Json& request_result, ctx::Json& data_read)
{
	IF_FAIL_RETURN(!replied && _ctx_monitor, true);
	_ctx_monitor->reply_result(_req_id, error, _subject.c_str(), &get_description(), &data_read);
	return (replied = true);
}

bool ctx::fact_request::publish(int error, ctx::Json& data)
{
	IF_FAIL_RETURN(_ctx_monitor, true);
	_ctx_monitor->publish_fact(_req_id, error, _subject.c_str(), &get_description(), &data);
	return true;
}
