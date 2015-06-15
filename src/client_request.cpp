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

#include <unistd.h>
#include <glib.h>
#include <security-server.h>
#include <app_manager.h>
#include <types_internal.h>
#include <dbus_server.h>
#include "zone_util_impl.h"
#include "dbus_server_impl.h"
#include "access_control/privilege.h"
#include "client_request.h"

ctx::client_request::client_request(int type, const char* client, int req_id, const char* subj, const char* desc, const char* cookie, GDBusMethodInvocation *inv)
	: request_info(type, client, req_id, subj, desc)
	, invocation(inv)
{
	gsize size;
	int client_pid;
	char *decoded = NULL;
	const char *zone_name = NULL;
	char *pkg_id = NULL;

	decoded = reinterpret_cast<char*>(g_base64_decode(cookie, &size));
	IF_FAIL_CATCH_TAG(decoded, _E, "Cookie decoding failed");

	raw_cookie = decoded;
	client_pid = security_server_get_cookie_pid(decoded);
	pkg_id = security_server_get_smacklabel_cookie(decoded);
	g_free(decoded);
	IF_FAIL_CATCH_TAG(client_pid > 0, _E, "Invalid PID (%d)", client_pid);

	if (pkg_id == NULL) {
		_W(RED("security_server_get_smacklabel_cookie() failed"));
		char* app_id = NULL;
		app_manager_get_app_id(client_pid, &app_id);
		client_app_id = ctx::privilege_manager::get_pkg_id(app_id);
		g_free(app_id);
	} else {
		//FIXME: Yes.. this is actually the package id
		client_app_id = pkg_id;
		g_free(pkg_id);
	}

	zone_name = ctx::zone_util::get_name_by_pid(client_pid);
	IF_FAIL_CATCH_TAG(zone_name, _E, RED("Zone name retrieval failed"));
	_zone_name = zone_name;

	_SD(CYAN("Package: '%s' / Zone: '%s'"), client_app_id.c_str(), zone_name);
	return;

CATCH:
	invocation = NULL;
	throw ERR_OPERATION_FAILED;
}

ctx::client_request::~client_request()
{
	if (invocation)
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(iss)", ERR_OPERATION_FAILED, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));
}

const char* ctx::client_request::get_cookie()
{
	return raw_cookie.c_str();
}

const char* ctx::client_request::get_app_id()
{
	if (!client_app_id.empty())
		return client_app_id.c_str();

	return NULL;
}

bool ctx::client_request::reply(int error)
{
	IF_FAIL_RETURN(invocation, true);

	_I("Reply %#x", error);

	g_dbus_method_invocation_return_value(invocation, g_variant_new("(iss)", error, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));
	invocation = NULL;
	return true;
}

bool ctx::client_request::reply(int error, ctx::json& request_result)
{
	IF_FAIL_RETURN(invocation, true);
	IF_FAIL_RETURN(_type != REQ_READ_SYNC, true);

	char *result = request_result.dup_cstr();
	IF_FAIL_RETURN_TAG(result, false, _E, "Memory allocation failed");

	_I("Reply %#x", error);
	_SD("Result: %s", result);

	g_dbus_method_invocation_return_value(invocation, g_variant_new("(iss)", error, result, EMPTY_JSON_OBJECT));
	invocation = NULL;

	g_free(result);
	return true;
}

bool ctx::client_request::reply(int error, ctx::json& request_result, ctx::json& data_read)
{
	if (invocation == NULL) {
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

	g_dbus_method_invocation_return_value(invocation, g_variant_new("(iss)", error, result, data));
	invocation = NULL;

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

	dbus_server::publish(_client.c_str(), _req_id, _subject.c_str(), error, data_str);
	g_free(data_str);

	return true;
}
