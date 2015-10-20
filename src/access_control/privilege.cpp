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
#include <types_internal.h>
#include "peer_creds.h"
#include "privilege.h"

class permission_checker {
private:
	cynara *__cynara;

	permission_checker()
	{
		if (cynara_initialize(&__cynara, NULL) != CYNARA_API_SUCCESS) {
			_E("Cynara initialization failed");
			__cynara = NULL;
			return;
		}
		_I("Cynara initialized");
	}

	~permission_checker()
	{
		if (__cynara)
			cynara_finish(__cynara);

		_I("Cynara deinitialized");
	}

public:
	static permission_checker& get_instance()
	{
		static permission_checker instance;
		return instance;
	}

	bool has_permission(const ctx::credentials *creds, const char *privilege)
	{
		IF_FAIL_RETURN_TAG(__cynara, false, _E, "Cynara not initialized");
		int ret = cynara_check(__cynara, creds->client, creds->session, creds->user, privilege);
		return (ret == CYNARA_API_ACCESS_ALLOWED);
	}
};

bool ctx::privilege_manager::is_allowed(const ctx::credentials *creds, const char *privilege)
{
	IF_FAIL_RETURN(creds && privilege, true);

	std::string priv = "http://tizen.org/privilege/";
	priv += privilege;

	return permission_checker::get_instance().has_permission(creds, priv.c_str());
}
