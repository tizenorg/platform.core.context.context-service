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

#ifndef _CONTEXT_PROVIDER_LOADER_H_
#define _CONTEXT_PROVIDER_LOADER_H_

#include <map>

namespace ctx {

	class ContextProvider;

	struct CompareSubjectName {
		bool operator()(const char *left, const char *right) const {
			const char *pl = left;
			const char *pr = right;
			while (pl != NULL && pr != NULL) {
				if (*pl < *pr)
					return true;
				if (*pl > *pr)
					return false;
				++pl;
				++pr;
			}
			return false;
		}
	};

	class ProviderLoader {
	public:
		ProviderLoader();
		~ProviderLoader();

		ContextProvider* load(const char *subject);
		bool loadAll();

		static bool init();
		static bool popTriggerTemplate(std::string &subject, int &operation, Json &attribute, Json &option);

	private:
		ContextProvider* __load(const char *soPath, const char *subject);
		void __unload();

		static std::map<const char*, const char*, CompareSubjectName> __providerLibMap;
	};

}

#endif	/* _CONTEXT_PROVIDER_LOADER_H_ */
