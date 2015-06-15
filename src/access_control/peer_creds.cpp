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
#include <types_internal.h>
#include "peer_creds.h"

std::string ctx::peer_creds::get_smack_label(GDBusConnection *connection, const char *unique_name)
{
	gchar *client = NULL;
	int err = cynara_creds_gdbus_get_client(connection, unique_name, CLIENT_METHOD_SMACK, &client);
	IF_FAIL_RETURN_TAG(err == CYNARA_API_SUCCESS, "", _E, "cynara_creds_gdbus_get_client() failed");

	std::string ret = client;
	g_free(client);
	return ret;
}

pid_t ctx::peer_creds::get_pid(GDBusConnection *connection, const char *unique_name)
{
	pid_t pid = -1;
	int err = cynara_creds_gdbus_get_pid(connection, unique_name, &pid);
	IF_FAIL_RETURN_TAG(err == CYNARA_API_SUCCESS, -1, _E, "cynara_creds_gdbus_get_pid() failed");

	return pid;
}
