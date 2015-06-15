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
#include <vasum.h>
#include <system_info.h>
#include <types_internal.h>
#include "zone_util_impl.h"

#define HOST_NAME ""

static bool container_enabled = false;
static vsm_context_h _vsm_ctx = NULL;
static ctx::zone_util_impl* _instance = NULL;

const char* ctx::zone_util_impl::default_zone()
{
	if (container_enabled)
		return VSM_DEFAULT_ZONE;

	return HOST_NAME;
}

void* ctx::zone_util_impl::join_by_name(const char* name)
{
	IF_FAIL_RETURN(container_enabled, NULL);
	IF_FAIL_RETURN_TAG(_vsm_ctx, NULL, _E, "Not initialized");

	if (name == NULL) {
		_D("NULL zone name. The default zone will be used.");
		name = VSM_DEFAULT_ZONE;
	}

	vsm_zone_h target_zone = vsm_lookup_zone_by_name(_vsm_ctx, name);
	IF_FAIL_RETURN_TAG(target_zone, NULL, _E, RED("Zone lookup failed"));

	vsm_zone_h current_zone = vsm_lookup_zone_by_pid(_vsm_ctx, getpid());
	IF_FAIL_RETURN_TAG(current_zone, NULL, _E, RED("Zone lookup failed"));
	IF_FAIL_RETURN_TAG(target_zone != current_zone, NULL, _I, YELLOW("Already in the target zone %s"), name);

	_I(YELLOW("Joining to '%s'"), name);
	return vsm_join_zone(target_zone);
}

void* ctx::zone_util_impl::join_to_zone(void* zone)
{
	IF_FAIL_RETURN(container_enabled, NULL);
	IF_FAIL_RETURN_TAG(_vsm_ctx, NULL, _E, "Not initialized");
	IF_FAIL_RETURN(zone, NULL);
	vsm_zone_h target = static_cast<vsm_zone_h>(zone);
	_I(YELLOW("Joining to '%s'"), vsm_get_zone_name(target));
	return vsm_join_zone(target);
}

bool ctx::zone_util::init()
{
	system_info_get_platform_bool("tizen.org/feature/container", &container_enabled);
	IF_FAIL_RETURN_TAG(_instance == NULL, true, _W, "Re-initialization");

	_instance = new(std::nothrow) zone_util_impl();
	IF_FAIL_RETURN_TAG(_instance, false, _E, "Memory allocation failed");

	if (container_enabled) {
		_vsm_ctx = vsm_create_context();
		if (!_vsm_ctx) {
			delete _instance;
			_E("Memory allocation failed");
			return false;
		}
	}

	zone_util::set_instance(_instance);
	return true;
}

void ctx::zone_util::release()
{
	zone_util::set_instance(NULL);

	delete _instance;
	_instance = NULL;

	if (_vsm_ctx)
		vsm_cleanup_context(_vsm_ctx);

	_vsm_ctx = NULL;
}

const char* ctx::zone_util::get_name_by_pid(pid_t pid)
{
	IF_FAIL_RETURN(container_enabled, HOST_NAME);
	IF_FAIL_RETURN_TAG(_vsm_ctx, NULL, _E, "Not initialized");
	vsm_zone_h zn = vsm_lookup_zone_by_pid(_vsm_ctx, pid);
	return vsm_get_zone_name(zn);
}
