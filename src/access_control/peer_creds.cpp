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

#include <cynara-creds-gdbus.h>
#include <app_manager.h>
#include <types_internal.h>
#include "peer_creds.h"

ctx::credentials::credentials(char *_app_id, char *_client) :
	app_id(_app_id),
	client(_client)
{
}

ctx::credentials::~credentials()
{
	g_free(app_id);
	g_free(client);
}

bool ctx::peer_creds::get(GDBusConnection *connection, const char *unique_name, ctx::credentials **creds)
{
	pid_t pid = 0;
	char *app_id = NULL;
	gchar *client = NULL;
	int err;

	err = cynara_creds_gdbus_get_pid(connection, unique_name, &pid);
	IF_FAIL_RETURN_TAG(err == CYNARA_API_SUCCESS, false, _E, "Peer credentialing failed");

	err = cynara_creds_gdbus_get_client(connection, unique_name, CLIENT_METHOD_DEFAULT, &client);
	IF_FAIL_CATCH_TAG(err == CYNARA_API_SUCCESS, _E, "Peer credentialing failed");

	/* TODO: session & user */

	app_manager_get_app_id(pid, &app_id);
	_D("AppId: %s", app_id);

	*creds = new(std::nothrow) credentials(app_id, client);
	IF_FAIL_CATCH_TAG(*creds, _E, "Memory allocation failed");

	return true;

CATCH:
	g_free(app_id);
	g_free(client);
	return false;
}
