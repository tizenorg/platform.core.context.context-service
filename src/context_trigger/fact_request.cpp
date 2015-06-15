/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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

ctx::fact_request::fact_request(int type, const char* client, int req_id, const char* subj, const char* desc, const char* zone, fact_reader* reader)
	: request_info(type, client, req_id, subj, desc)
	, _reader(reader)
	, replied(false)
{
	if (zone)
		_zone_name = zone;
}

ctx::fact_request::~fact_request()
{
	reply(ERR_OPERATION_FAILED);
}

bool ctx::fact_request::reply(int error)
{
	IF_FAIL_RETURN(!replied && _reader, true);
	_reader->reply_result(_req_id, error, _zone_name.c_str());
	return (replied = true);
}

bool ctx::fact_request::reply(int error, ctx::json& request_result)
{
	IF_FAIL_RETURN(!replied && _reader, true);
	IF_FAIL_RETURN(_type != REQ_READ_SYNC, true);
	_reader->reply_result(_req_id, error, _zone_name.c_str(), &request_result);
	return (replied = true);
}

bool ctx::fact_request::reply(int error, ctx::json& request_result, ctx::json& data_read)
{
	IF_FAIL_RETURN(!replied && _reader, true);
	_reader->reply_result(_req_id, error, _zone_name.c_str(), &request_result, &data_read);
	return (replied = true);
}

bool ctx::fact_request::publish(int error, ctx::json& data)
{
	IF_FAIL_RETURN(_reader, true);
	_reader->publish_fact(_req_id, error, _zone_name.c_str(), _subject.c_str(), &get_description(), &data);
	return true;
}

void ctx::fact_request::set_zone_name(const char* zone_name)
{
	if (zone_name)
		_zone_name = zone_name;
}
