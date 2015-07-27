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

#include <unistd.h>
#include <glib.h>
#include <app_manager.h>
#include <types_internal.h>
#include <dbus_server.h>
#include "dbus_server_impl.h"
#include "client_request.h"

ctx::client_request::client_request(int type,
		const char *client, int req_id, const char *subj, const char *desc,
		const char *sender, GDBusMethodInvocation *inv)
	: request_info(type, client, req_id, subj, desc)
	, __sender(sender)
	, __invocation(inv)
{
}

ctx::client_request::~client_request()
{
	if (__invocation)
		g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", ERR_OPERATION_FAILED, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));
}

bool ctx::client_request::reply(int error)
{
	IF_FAIL_RETURN(__invocation, true);

	_I("Reply %#x", error);

	g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", error, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));
	__invocation = NULL;
	return true;
}

bool ctx::client_request::reply(int error, ctx::json& request_result)
{
	IF_FAIL_RETURN(__invocation, true);
	IF_FAIL_RETURN(_type != REQ_READ_SYNC, true);

	char *result = request_result.dup_cstr();
	IF_FAIL_RETURN_TAG(result, false, _E, "Memory allocation failed");

	_I("Reply %#x", error);
	_SD("Result: %s", result);

	g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", error, result, EMPTY_JSON_OBJECT));
	__invocation = NULL;

	g_free(result);
	return true;
}

bool ctx::client_request::reply(int error, ctx::json& request_result, ctx::json& data_read)
{
	if (__invocation == NULL) {
		return publish(error, data_read);
	}

	char *result = NULL;
	char *data = NULL;

	result = request_result.dup_cstr();
	IF_FAIL_CATCH_TAG(result, _E, "Memory allocation failed");

	data = data_read.dup_cstr();
	IF_FAIL_CATCH_TAG(data, _E, "Memory allocation failed");

	_I("Reply %#x", error);
	_SD("Result: %s", result);
	_SD("Data: %s", data);

	g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", error, result, data));
	__invocation = NULL;

	g_free(result);
	g_free(data);
	return true;

CATCH:
	g_free(result);
	g_free(data);
	return false;
}

bool ctx::client_request::publish(int error, ctx::json& data)
{
	char *data_str = data.dup_cstr();
	IF_FAIL_RETURN_TAG(data_str, false, _E, "Memory allocation failed");

	dbus_server::publish(__sender.c_str(), _req_id, _subject.c_str(), error, data_str);
	g_free(data_str);

	return true;
}
