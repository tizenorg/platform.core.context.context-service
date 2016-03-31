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
#include "ClientRequest.h"

ctx::ClientRequest::ClientRequest(int type, int reqId, const char *subj, const char *desc,
		ctx::credentials *creds, const char *sender, GDBusMethodInvocation *inv) :
	RequestInfo(type, reqId, subj, desc),
	__credentials(creds),
	__dbusSender(sender),
	__invocation(inv)
{
}

ctx::ClientRequest::~ClientRequest()
{
	if (__invocation)
		g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", ERR_OPERATION_FAILED, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));

	delete __credentials;
}

const ctx::credentials* ctx::ClientRequest::getCredentials()
{
	return __credentials;
}

const char* ctx::ClientRequest::getPackageId()
{
	if (__credentials)
		return __credentials->package_id;

	return NULL;
}

const char* ctx::ClientRequest::getClient()
{
	if (__credentials)
		return __credentials->client;

	return NULL;
}

bool ctx::ClientRequest::reply(int error)
{
	IF_FAIL_RETURN(__invocation, true);

	_I("Reply %#x", error);

	g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", error, EMPTY_JSON_OBJECT, EMPTY_JSON_OBJECT));
	__invocation = NULL;
	return true;
}

bool ctx::ClientRequest::reply(int error, ctx::Json& requestResult)
{
	IF_FAIL_RETURN(__invocation, true);
	IF_FAIL_RETURN(_type != REQ_READ_SYNC, true);

	std::string result = requestResult.str();
	IF_FAIL_RETURN(!result.empty(), false);

	_I("Reply %#x", error);
	_SD("Result: %s", result.c_str());

	g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", error, result.c_str(), EMPTY_JSON_OBJECT));
	__invocation = NULL;

	return true;
}

bool ctx::ClientRequest::reply(int error, ctx::Json& requestResult, ctx::Json& dataRead)
{
	if (__invocation == NULL) {
		return publish(error, dataRead);
	}

	std::string result = requestResult.str();
	std::string data = dataRead.str();
	IF_FAIL_RETURN(!result.empty() && !data.empty(), false);

	_I("Reply %#x", error);
	_SD("Result: %s", result.c_str());
	_SD("Data: %s", data.c_str());

	g_dbus_method_invocation_return_value(__invocation, g_variant_new("(iss)", error, result.c_str(), data.c_str()));
	__invocation = NULL;

	return true;
}

bool ctx::ClientRequest::publish(int error, ctx::Json& data)
{
	DBusServer::publish(__dbusSender, _reqId, _subject, error, data.str());
	return true;
}
