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

#include <string>
#include <cynara-client.h>
#include <Types.h>
#include "PeerCreds.h"
#include "Privilege.h"

#define CACHE_SIZE 100

class PermissionChecker {
private:
	cynara *__cynara;

	PermissionChecker()
	{
		cynara_configuration *conf;

		int err = cynara_configuration_create(&conf);
		IF_FAIL_VOID_TAG(err == CYNARA_API_SUCCESS, _E, "Cynara configuration creation failed");

		err = cynara_configuration_set_cache_size(conf, CACHE_SIZE);
		if (err != CYNARA_API_SUCCESS) {
			_E("Cynara cache size set failed");
			cynara_configuration_destroy(conf);
			return;
		}

		err = cynara_initialize(&__cynara, conf);
		cynara_configuration_destroy(conf);
		if (err != CYNARA_API_SUCCESS) {
			_E("Cynara initialization failed");
			__cynara = NULL;
			return;
		}

		_I("Cynara initialized");
	}

	~PermissionChecker()
	{
		if (__cynara)
			cynara_finish(__cynara);

		_I("Cynara deinitialized");
	}

public:
	static PermissionChecker& getInstance()
	{
		static PermissionChecker instance;
		return instance;
	}

	bool hasPermission(const ctx::Credentials *creds, const char *privilege)
	{
		IF_FAIL_RETURN_TAG(__cynara, false, _E, "Cynara not initialized");
		int ret = cynara_check(__cynara, creds->client, creds->session, creds->user, privilege);
		return (ret == CYNARA_API_ACCESS_ALLOWED);
	}
};

bool ctx::privilege_manager::isAllowed(const ctx::Credentials *creds, const char *privilege)
{
	IF_FAIL_RETURN(creds && privilege, true);

	std::string priv = "http://tizen.org/privilege/";
	priv += privilege;

	return PermissionChecker::getInstance().hasPermission(creds, priv.c_str());
}
