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
#include <Types.h>
#include "PeerCreds.h"
#include "Privilege.h"

#ifdef LEGACY_SECURITY

#include <sys/smack.h>
#define PRIV_PREFIX "privilege::tizen::"

#else

#include <cynara-client.h>
#define PRIV_PREFIX "http://tizen.org/privilege/"
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
			_E("Cynara cache size set failed");	//LCOV_EXCL_LINE
			cynara_configuration_destroy(conf);
			return;
		}

		err = cynara_initialize(&__cynara, conf);
		cynara_configuration_destroy(conf);
		if (err != CYNARA_API_SUCCESS) {
			_E("Cynara initialization failed");	//LCOV_EXCL_LINE
			__cynara = NULL;
			return;
		}

		_I("Cynara initialized");
	}
	//LCOV_EXCL_START
	~PermissionChecker()
	{
		if (__cynara)
			cynara_finish(__cynara);

		_I("Cynara deinitialized");
	}
	//LCOV_EXCL_STOP

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
#endif

bool ctx::privilege_manager::isAllowed(const ctx::Credentials *creds, const char *privilege)
{
	IF_FAIL_RETURN(creds && privilege, true);

	std::string priv = PRIV_PREFIX;
	priv += privilege;

#ifdef LEGACY_SECURITY
	int ret = smack_have_access(creds->client, priv.c_str(), "rw");
	_SD("Client: %s, Priv: %s, Enabled: %d", creds->client, privilege, ret);
	return (ret == 1);
#else
	return PermissionChecker::getInstance().hasPermission(creds, priv.c_str());
#endif
}
