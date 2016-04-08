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

#ifndef _CONTEXT_TRIGGER_RULE_MANAGER_H_
#define _CONTEXT_TRIGGER_RULE_MANAGER_H_

#include <set>
#include <map>

namespace ctx {

	class Json;

namespace trigger {

	class Trigger;
	class Rule;

	class RuleManager {
		public:
			RuleManager();
			~RuleManager();

			bool init();
			int addRule(std::string creator, const char* pkgId, Json rule, Json* ruleId);
			int removeRule(int ruleId);
			int enableRule(int ruleId);
			int disableRule(int ruleId);
			int getRuleById(std::string pkgId, int ruleId, Json* requestResult);
			int getRuleIds(std::string pkgId, Json* requestResult);
			int checkRule(std::string pkgId, int ruleId);
			bool isRuleEnabled(int ruleId);
			int pauseRuleWithItem(std::string& subject);
			int pauseRule(int ruleId);
			int resumeRuleWithItem(std::string& subject);
			void handleRuleOfUninstalledPackage(std::string pkgId);

			static bool isUninstalledPackage(std::string pkgId);

		private:
			bool __reenableRule(void);
			int __verifyRule(Json& rule, const char* creator);
			int64_t __getDuplicatedRuleId(std::string pkgId, Json& rule);
			bool __ruleDataArrElemEquals(Json& lElem, Json& rElem);
			bool __ruleItemEquals(Json& lItem, Json& rItem);
			bool __ruleEquals(Json& lRule, Json& rRule);
			int __getUninstalledApp(void);
			int __clearRuleOfUninstalledPackage(bool isInit = false);
			void __applyTemplates(void);

			std::set<std::string> __uninstalledPackages;

			std::map<int, Rule*> __ruleMap;
   };	/* class RuleManager */

}	/* namespace trigger */
}	/* namespace ctx */

#endif	/* End of _CONTEXT_TRIGGER_RULE_MANAGER_H_ */
