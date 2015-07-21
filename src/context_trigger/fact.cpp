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
#include "fact.h"

ctx::context_fact::context_fact()
	: req_id(-1)
	, error(ERR_NONE)
{
}

ctx::context_fact::context_fact(int id, int err, const char* s, ctx::json& o, ctx::json& d)
	: req_id(id)
	, error(err)
	, subject(s)
	, option(o)
	, data(d)
{
}

ctx::context_fact::~context_fact()
{
}

void ctx::context_fact::set_req_id(int id)
{
	req_id = id;
}

void ctx::context_fact::set_error(int err)
{
	error = err;
}

void ctx::context_fact::set_subject(const char* s)
{
	subject = s;
}

void ctx::context_fact::set_option(ctx::json& o)
{
	option = o;
}

void ctx::context_fact::set_data(ctx::json& d)
{
	data = d;
}

int ctx::context_fact::get_req_id()
{
	return req_id;
}

int ctx::context_fact::get_error()
{
	return error;
}

const char* ctx::context_fact::get_subject()
{
	return subject.c_str();
}

ctx::json& ctx::context_fact::get_option()
{
	return option;
}

ctx::json& ctx::context_fact::get_data()
{
	return data;
}
