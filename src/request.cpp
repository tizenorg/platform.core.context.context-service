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
#include <types_internal.h>
#include "request.h"

ctx::request_info::request_info(int type, const char* client, int req_id, const char* subj, const char* desc)
	: _type(type)
	, _req_id(req_id)
	, _client(client)
	, _subject(subj)
	, _description(desc)
{
}

ctx::request_info::~request_info()
{
}

int ctx::request_info::get_type()
{
	return _type;
}

int ctx::request_info::get_id()
{
	return _req_id;
}

const char* ctx::request_info::get_client()
{
	return _client.c_str();
}

const char* ctx::request_info::get_subject()
{
	return _subject.c_str();
}

ctx::json& ctx::request_info::get_description()
{
	return _description;
}
