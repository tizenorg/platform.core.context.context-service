/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#include <dlfcn.h>
#include <dirent.h>
#include <set>
#include <Types.h>
#include <ContextProvider.h>
#include "ProviderLoader.h"

#define PROVIDER_SO_PATH "/usr/lib/context"

using namespace ctx;

typedef bool (*create_t)();

static bool getSharedObjectPaths(const char *dirPath, std::set<std::string> &soNames)
{
	DIR *dir = NULL;
	struct dirent dirEntry;
	struct dirent *result;
	int error;

	dir = opendir(dirPath);
	IF_FAIL_RETURN_TAG(dir, false, _E, "Failed to open '%s'", dirPath);

	while (true) {
		error = readdir_r(dir, &dirEntry, &result);

		if (error != 0)
			continue;

		if (result == NULL)
			break;

		if (dirEntry.d_type != DT_REG)
			continue;

		soNames.insert(dirEntry.d_name);
	}

	closedir(dir);
	return true;
}

ProviderLoader::ProviderLoader()
{
}

ProviderLoader::~ProviderLoader()
{
	__unload();
}

ContextProvider* ProviderLoader::load(const char *subject)
{
	/* TODO: Implement */
	return NULL;
}

ContextProvider* ProviderLoader::__load(const char *soPath, const char *subject)
{
	_I("Load '%s' from '%s'", subject, soPath);

	void *soHandle = dlopen(soPath, RTLD_LAZY | RTLD_GLOBAL);
	IF_FAIL_RETURN_TAG(soHandle, NULL, _E, "%s", dlerror());

	create_t create = reinterpret_cast<create_t>(dlsym(soHandle, "create"));
	if (!create) {
		_E("%s", dlerror());
		dlclose(soHandle);
		return NULL;
	}

	/* TODO: Update this part for dynamic loading */
	create();

	return NULL;
}

void ProviderLoader::__unload()
{
	/* TODO: Implement */
}

bool ProviderLoader::loadAll()
{
	/* TODO: Remove this function. This is a temporary solution. */
	std::set<std::string> soNames;
	std::string soPath;

	if (!getSharedObjectPaths(PROVIDER_SO_PATH, soNames)) {
		return false;
	}

	for (std::set<std::string>::iterator it = soNames.begin(); it != soNames.end(); ++it) {
		soPath = PROVIDER_SO_PATH;
		soPath = soPath + "/" + (*it);
		__load(soPath.c_str(), NULL);
	}

	return true;
}
