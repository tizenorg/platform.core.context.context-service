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
#include <types_internal.h>
#include "privilege.h"

bool ctx::privilege_manager::is_allowed(const char *client, const char *privilege)
{
	/* TODO: need to be implemented using Cynara */
#if 0
	IF_FAIL_RETURN(privilege, true);

	std::string priv = "privilege::tizen::";
	priv += privilege;

	int ret = smack_have_access(client, priv.c_str(), "rw");
	_D("Client: %s, Priv: %s, Enabled: %d", client, privilege, ret);

	return (ret == 1);
#endif
	return true;
}
