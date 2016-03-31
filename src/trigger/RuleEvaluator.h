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

#ifndef _CONTEXT_TRIGGER_RULE_EVALUATOR_H_
#define _CONTEXT_TRIGGER_RULE_EVALUATOR_H_

namespace ctx {

	class Json;

namespace trigger {

	class RuleEvaluator {
	private:
		RuleEvaluator();

		bool __evaluateRuleEvent(ctx::Json& rule, ctx::Json& fact);
		bool __evaluateRuleCondition(ctx::Json& rule, ctx::Json& fact);
		bool __evaluateItem(ctx::Json& ruleItem, ctx::Json& factItem);
		bool __evaluateDataString(ctx::Json& ruleDataArr, std::string factValStr);
		bool __evaluateDataInt(ctx::Json& ruleDataArr, int factValInt);
		bool __compareInt(std::string op, int ruleVar, int factVar);
		bool __compareString(std::string op, std::string ruleVar, std::string factVar);

		ctx::Json __getConditionFact(ctx::Json& ruleCond, ctx::Json& fact);

		bool __replaceEventReferences(ctx::Json& rule, ctx::Json& fact);
		bool __replaceDataReferences(ctx::Json& ruleDataArr, ctx::Json eventFactData);
		bool __replaceOptionReferences(ctx::Json& ruleOption, ctx::Json eventFactData);

	public:
		static bool evaluateRule(ctx::Json rule, ctx::Json data);
	};

}	/* namespace trigger */
}	/* namespace ctx */

#endif	/* End of _CONTEXT_TRIGGER_RULE_EVALUATOR_H_ */
