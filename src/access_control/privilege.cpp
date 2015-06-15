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

#include <string>
#include <map>

#include <pkgmgr-info.h>
#include <privilege_checker.h>

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
	IF_FAIL_RETURN_TAG(pkg_id && subject, true, _E, "Invalid parameter");
	IF_FAIL_RETURN_TAG(pkg_id[0]!='\0' && subject[0]!='\0', true, _E, "Invalid parameter");

	string_map_t::iterator it = privilege_map->find(subject);
	if (it == privilege_map->end()) {
		// Non-Privileged Subject
		return true;
	}

	_D("PkgId: %s, Priv: %s", pkg_id, (it->second).c_str());
	std::string priv = "http://tizen.org/privilege/";
	priv += (it->second).c_str();
	int ret = privilege_checker_check_package_privilege(pkg_id, priv.c_str());
	_D("Privilege Check Result: %#x", ret);
	return (ret == PRIV_CHECKER_ERR_NONE);
}

std::string ctx::privilege_manager::get_pkg_id(const char* app_id)
{
	std::string pkg_id;
	IF_FAIL_RETURN_TAG(app_id, pkg_id, _E, "Null AppId");

	int ret;
	pkgmgrinfo_appinfo_h app_info;

	ret = pkgmgrinfo_appinfo_get_appinfo(app_id, &app_info);
	IF_FAIL_RETURN_TAG(ret == PMINFO_R_OK, pkg_id, _E, "Failed to get app_info");

	char *pkg_name = NULL;
	ret = pkgmgrinfo_appinfo_get_pkgname(app_info, &pkg_name);
	if (ret != PMINFO_R_OK || pkg_name == NULL) {
		pkgmgrinfo_appinfo_destroy_appinfo(app_info);
		_E("Failed to get package name");
		return pkg_id;
	}

	pkg_id = pkg_name;
	pkgmgrinfo_appinfo_destroy_appinfo(app_info);
	return pkg_id;
}
