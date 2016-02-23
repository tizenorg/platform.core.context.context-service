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
#include "DBusServer.h"
#include "access_control/peer_creds.h"
#include "client_request.h"

ctx::client_request::client_request(int type, int req_id, const char *subj, const char *desc,
		ctx::credentials *creds, const char *sender, GDBusMethodInvocation *inv)
	: request_info(type, req_id, subj, desc)
	, __credentials(creds)
	, __dbus_sender(sender)
	, __invocation(inv)
{
}

ctx::client_request::~client_request()
{
	if (__invocation)
		g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", ERR_OPERATION_FAILED, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));

	delete __credentials;
}

const ctx::credentials* ctx::client_request::get_credentials()
{
	return __credentials;
}

const char* ctx::client_request::get_package_id()
{
	if (__credentials)
		return __credentials->package_id;

	return NULL;
}

const char* ctx::client_request::get_client()
{
	if (__credentials)
		return __credentials->client;

	return NULL;
}

bool ctx::client_request::reply(int error)
{
	IF_FAIL_RETURN(__invocation, true);

	_I("Reply %#x", error);

	g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", error, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));
	__invocation = NULL;
	return true;
}

bool ctx::client_request::reply(int error, ctx::Json& request_result)
{
	IF_FAIL_RETURN(__invocation, true);
	IF_FAIL_RETURN(_type != REQ_READ_SYNC, true);

	std::string result = request_result.str();
	IF_FAIL_RETURN(!result.empty(), false);

	_I("Reply %#x", error);
	_SD("Result: %s", result.c_str());

	g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", error, result.c_str(), EMPTY_JSON_OBJECT));
	__invocation = NULL;

	return true;
}

bool ctx::client_request::reply(int error, ctx::Json& request_result, ctx::Json& data_read)
{
	if (__invocation == NULL) {
		return publish(error, data_read);
	}

	std::string result = request_result.str();
	std::string data = data_read.str();
	IF_FAIL_RETURN(!result.empty() && !data.empty(), false);

	_I("Reply %#x", error);
	_SD("Result: %s", result.c_str());
	_SD("Data: %s", data.c_str());

	g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", error, result.c_str(), data.c_str()));
	__invocation = NULL;

	return true;
}

bool ctx::client_request::publish(int error, ctx::Json& data)
{
	DBusServer::publish(__dbus_sender, _req_id, _subject, error, data.str());
	return true;
}
