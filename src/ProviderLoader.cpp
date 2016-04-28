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

#include <set>
#include <Types.h>
#include <ContextProvider.h>
#include <ProviderList.h>
#include "ProviderLoader.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

using namespace ctx;

typedef ContextProvider* (*create_t)(const char *subject);

std::map<const char*, const char*, CompareSubjectName> ProviderLoader::__providerLibMap;

ProviderLoader::ProviderLoader() :
	__soHandle(NULL)
{
}

ProviderLoader::~ProviderLoader()
{
	__unload();
}

ContextProvider* ProviderLoader::load(const char *subject)
{
	ProviderLibMap::iterator it = __providerLibMap.find(subject);
	if (it == __providerLibMap.end()) {
		_W("No provider for '%s'", subject);
		return NULL;
	}

	std::string path = LIB_DIRECTORY;
	path = path + LIB_PREFIX + it->second + LIB_EXTENSION;

	return __load(path.c_str(), subject);
}

ContextProvider* ProviderLoader::__load(const char *soPath, const char *subject)
{
	_SI("Load '%s' from '%s'", subject, soPath);

	__soHandle = g_module_open(soPath, static_cast<GModuleFlags>(0));
	IF_FAIL_RETURN_TAG(__soHandle, NULL, _E, "%s", g_module_error());

	gpointer symbol;

	if (!g_module_symbol(__soHandle, "create", &symbol) || symbol == NULL) {
		_E("%s", g_module_error());
		g_module_close(__soHandle);
		__soHandle = NULL;
		return NULL;
	}

	create_t create = reinterpret_cast<create_t>(symbol);

	ContextProvider *prvd = create(subject);
	if (!prvd) {
		_W("No provider for '%s'", subject);
		g_module_close(__soHandle);
		__soHandle = NULL;
		return NULL;
	}

	return prvd;
}

void ProviderLoader::__unload()
{
	if (!__soHandle)
		return;

	g_module_close(__soHandle);
	__soHandle = NULL;
}

bool ProviderLoader::init()
{
	int size = ARRAY_SIZE(subjectLibraryList);

	for (int i = 0; i < size; ++i) {
		__providerLibMap[subjectLibraryList[i].subject] = subjectLibraryList[i].library;
		_SD("'%s' -> '%s'", subjectLibraryList[i].subject, subjectLibraryList[i].library);
	}

	return true;
}

bool ProviderLoader::popTriggerTemplate(std::string &subject, int &operation, Json &attribute, Json &option)
{
	static int i = 0;
	static int size = ARRAY_SIZE(triggerTemplateList);

	if (i == size)
		return false;

	subject = triggerTemplateList[i].subject;
	operation = triggerTemplateList[i].operation;
	attribute = triggerTemplateList[i].attribute;
	option = triggerTemplateList[i].option;

	++i;
	return true;
}
