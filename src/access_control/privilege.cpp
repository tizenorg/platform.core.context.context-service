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
#include <map>
#include <types_internal.h>
#include "config_loader.h"
#include "privilege.h"

typedef std::map<std::string, std::string> string_map_t;

static string_map_t *privilege_map = NULL;

bool ctx::privilege_manager::init()
{
	IF_FAIL_RETURN(privilege_map == NULL, true);

	privilege_map = new(std::nothrow) string_map_t;

	if (!ctx::access_config_loader::load()) {
		_E("Loading failed");
		delete privilege_map;
		return false;
	}

	return true;
}

void ctx::privilege_manager::release()
{
	delete privilege_map;
	privilege_map = NULL;
}

void ctx::privilege_manager::set(const char* subject, const char* priv)
{
	(*privilege_map)[subject] = priv;
}

bool ctx::privilege_manager::is_allowed(const char* pkg_id, const char* subject)
{
	/* TODO: need to be implemented using Cynara */
#if 0
	IF_FAIL_RETURN_TAG(pkg_id && subject, true, _E, "Invalid parameter");
	IF_FAIL_RETURN_TAG(pkg_id[0]!='\0' && subject[0]!='\0', true, _E, "Invalid parameter");

	string_map_t::iterator it = privilege_map->find(subject);
	if (it == privilege_map->end()) {
		// Non-Privileged Subject
		return true;
	}

	std::string priv = "org.tizen.privilege.";
	priv += (it->second).c_str();
	int result = 0;
	int err = security_server_app_has_privilege(pkg_id, PERM_APP_TYPE_EFL, priv.c_str(), &result);

	_D("PkgId: %s, PrivName: %s, Enabled: %d", pkg_id, (it->second).c_str(), result);
	IF_FAIL_RETURN_TAG(err == SECURITY_SERVER_API_SUCCESS, false, _E, "Privilege checking failed");

	return (result == 1);
#endif
	return true;
}
