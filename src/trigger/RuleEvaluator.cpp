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

#include <Json.h>
#include <Types.h>
#include <context_trigger_types_internal.h>
#include "RuleEvaluator.h"
#include "FactTypes.h"

#define AND_STRING "and"
#define OR_STRING "or"
#define EVENT_REFERENCE "?"
#define OPERATOR_EQ "=="
#define OPERATOR_NEQ "!="
#define OPERATOR_LEQ "<="
#define OPERATOR_GEQ ">="
#define OPERATOR_LT "<"
#define OPERATOR_GT ">"

using namespace ctx;
using namespace ctx::trigger;
//LCOV_EXCL_START
RuleEvaluator::RuleEvaluator()
{
}

bool RuleEvaluator::__compareString(std::string op, std::string ruleVar, std::string factVar)
{
	if (op == OPERATOR_EQ) {
		return (ruleVar == factVar);
	} else if (op == OPERATOR_NEQ) {
		return (ruleVar != factVar);
	} else {
		_E("Operator %s not supported", op.c_str());
		return false;
	}
}

bool RuleEvaluator::__compareInt(std::string op, int ruleVar, int factVar)
{
	if (op == OPERATOR_EQ) {
		return (ruleVar == factVar);
	} else if (op == OPERATOR_NEQ) {
		return (ruleVar != factVar);
	} else if (op == OPERATOR_LEQ) {
		return (ruleVar <= factVar);
	} else if (op == OPERATOR_GEQ) {
		return (ruleVar >= factVar);
	} else if (op == OPERATOR_LT) {
		return (ruleVar < factVar);
	} else if (op == OPERATOR_GT) {
		return (ruleVar > factVar);
	} else {
		_E("Operator %s not supported", op.c_str());
		return false;
	}
}

bool RuleEvaluator::__replaceDataReferences(Json& ruleDataArr, Json eventFactData)
{
	// Replace referencing data to actual value
	std::string refVal;
	std::string eventRefStr;
	int eventRefInt;

	for (int i = 0; i < ruleDataArr.getSize(NULL, CT_RULE_DATA_VALUE_ARR); i++) {
		if (!ruleDataArr.getAt(NULL, CT_RULE_DATA_VALUE_ARR, i, &refVal)) {
			continue;
		}
		if (refVal.substr(0, 1) != EVENT_REFERENCE) {
			continue;
		}

		std::string eventKey = refVal.substr(1, refVal.length() - 1);
		if (eventFactData.get(NULL, eventKey.c_str(), &eventRefStr)) {
			ruleDataArr.setAt(NULL, CT_RULE_DATA_VALUE_ARR, i, eventRefStr);
		} else if (eventFactData.get(NULL, eventKey.c_str(), &eventRefInt)) {
			ruleDataArr.setAt(NULL, CT_RULE_DATA_VALUE_ARR, i, eventRefInt);
		} else {
			_W("Option %s not found in event_data", eventKey.c_str());
		}
	}

	return true;
}

bool RuleEvaluator::__replaceOptionReferences(Json& ruleOption, Json eventFactData)
{
	// Replace referencing option to actual value
	std::string refVal;
	std::string eventRefStr;
	int eventRefInt;

	std::list<std::string> keyList;
	ruleOption.getKeys(&keyList);

	for (std::list<std::string>::iterator it = keyList.begin(); it != keyList.end(); ++it) {
		std::string optionKey = *it;

		if (!ruleOption.get(NULL, (*it).c_str(), &refVal)) {
			continue;
		}
		if (!(refVal.substr(0, 1) == EVENT_REFERENCE)) {
			continue;
		}

		std::string eventKey = refVal.substr(1, refVal.length() - 1);
		if (eventFactData.get(NULL, eventKey.c_str(), &eventRefStr)) {
			ruleOption.set(NULL, (*it).c_str(), eventRefStr);
		} else if (eventFactData.get(NULL, eventKey.c_str(), &eventRefInt)) {
			ruleOption.set(NULL, (*it).c_str(), eventRefInt);
		} else {
			_W("Option %s not found in event_data", eventKey.c_str());
			return false;
		}
	}

	return true;
}

bool RuleEvaluator::__replaceEventReferences(Json& rule, Json& fact)
{
	// Replace referencing data/option to actual value
	Json eventFactData;
	if (!fact.get(CONTEXT_FACT_EVENT, CONTEXT_FACT_DATA, &eventFactData)) {
		_E("No event data found, error");
		return false;
	}

	Json ruleCond;
	for (int i = 0; rule.getAt(NULL, CT_RULE_CONDITION, i, &ruleCond); i++) {
		Json ruleDataArr;
		for (int j = 0; ruleCond.getAt(NULL, CT_RULE_DATA_ARR, j, &ruleDataArr); j++) {
			__replaceDataReferences(ruleDataArr, eventFactData);
		}

		Json ruleOption;
		if (ruleCond.get(NULL, CT_RULE_CONDITION_OPTION, &ruleOption)) {
			__replaceOptionReferences(ruleOption, eventFactData);
		}
	}

	return true;
}

bool RuleEvaluator::__evaluateDataString(Json& ruleDataArr, std::string factValStr)
{
	std::string opVal;
	ruleDataArr.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &opVal);
	bool conjunction = (AND_STRING == opVal);

	std::string op;
	for (int i = 0; ruleDataArr.getAt(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, i, &op); i++) {
		bool result;
		std::string valStr;
		ruleDataArr.getAt(NULL, CT_RULE_DATA_VALUE_ARR, i, &valStr);
		result = __compareString(op, factValStr, valStr);

		if (conjunction && !result) {
			return false;
		} else if (!conjunction && result) {
			return true;
		}
	}
	return conjunction;
}

bool RuleEvaluator::__evaluateDataInt(Json& ruleDataArr, int factValInt)
{
	std::string opVal;
	ruleDataArr.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &opVal);
	bool conjunction = (AND_STRING == opVal);

	std::string op;
	for (int i = 0; ruleDataArr.getAt(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, i, &op); i++) {
		bool result;
		int valInt;
		if (!ruleDataArr.getAt(NULL, CT_RULE_DATA_VALUE_ARR, i, &valInt)) {
			result = false;
		}
		result = __compareInt(op, factValInt, valInt);

		if (conjunction && !result) {
			return false;
		} else if (!conjunction && result) {
			return true;
		}
	}
	return conjunction;
}

bool RuleEvaluator::__evaluateItem(Json& ruleItem, Json& factItem)
{
	Json ruleDataArr;
	if (ruleItem.getSize(NULL, CT_RULE_DATA_ARR) == 0) {
		return true;
	}

	std::string opKey;
	ruleItem.get(NULL, CT_RULE_CONDITION_OPERATOR, &opKey);
	bool conjunction = (AND_STRING == opKey);

	for (int i = 0; ruleItem.getAt(NULL, CT_RULE_DATA_ARR, i, &ruleDataArr); i++) {
		std::string dataKey;
		ruleDataArr.get(NULL, CT_RULE_DATA_KEY, &dataKey);
		std::string factValStr;
		int factValInt;

		bool result;
		if (factItem.get(CONTEXT_FACT_DATA, dataKey.c_str(), &factValStr)) {
			result = __evaluateDataString(ruleDataArr, factValStr);
		} else if (factItem.get(CONTEXT_FACT_DATA, dataKey.c_str(), &factValInt)) {
			result = __evaluateDataInt(ruleDataArr, factValInt);
		} else {
			_W("Could not get value corresponding to data key %s", dataKey.c_str());
			result = false;
		}

		if (conjunction && !result) {
			return false;
		} else if (!conjunction && result) {
			return true;
		}
	}
	return conjunction;
}

bool RuleEvaluator::__evaluateRuleEvent(Json& rule, Json& fact)
{
	Json factItem;
	Json ruleItem;
	fact.get(NULL, CONTEXT_FACT_EVENT, &factItem);
	rule.get(NULL, CT_RULE_EVENT, &ruleItem);

	return __evaluateItem(ruleItem, factItem);
}

Json RuleEvaluator::__getConditionFact(Json& ruleCond, Json& fact)
{
	std::string ruleCondName;
	ruleCond.get(NULL, CT_RULE_CONDITION_ITEM, &ruleCondName);

	Json factCond;
	for (int i = 0; fact.getAt(NULL, CONTEXT_FACT_CONDITION, i, &factCond); i++) {
		// Check if fact item name is matched with condition
		std::string factCondName;
		factCond.get(NULL, CONTEXT_FACT_NAME, &factCondName);
		if (factCondName != ruleCondName) {
			continue;
		}

		// Check if fact item option is mathced with condition
		Json ruleCondOption;
		Json factCondOption;
		ruleCond.get(NULL, CT_RULE_CONDITION_OPTION, &ruleCondOption);
		factCond.get(NULL, CONTEXT_FACT_OPTION, &factCondOption);
		if (factCondOption == ruleCondOption) {
			return factCond;
		}
	}

	_W(YELLOW("find condition failed for condition"));
	return EMPTY_JSON_OBJECT;
}

bool RuleEvaluator::__evaluateRuleCondition(Json& rule, Json& fact)
{
	Json ruleCond;
	Json factCond;

	std::string opCond;
	rule.get(NULL, CT_RULE_OPERATOR, &opCond);
	bool conjunction = (AND_STRING == opCond);

	for (int i = 0; rule.getAt(NULL, CT_RULE_CONDITION, i, &ruleCond); i++) {
		factCond = __getConditionFact(ruleCond, fact);
		bool result;
		if (factCond == EMPTY_JSON_OBJECT) {
			result = false;
		} else {
			result = __evaluateItem(ruleCond, factCond);
		}

		if (conjunction && !result) {
			return false;
		} else if (!conjunction && result) {
			return true;
		}
	}

	return conjunction;
}

bool RuleEvaluator::evaluateRule(Json rule, Json fact)
{
	_D("Rule is %s ", rule.str().c_str());
	_D("fact is %s ", fact.str().c_str());

	RuleEvaluator eval;
	bool ret;
	Json tempJson;
	if (fact.get(NULL, CT_RULE_CONDITION, &tempJson)) {
		Json ruleCopy(rule.str());
		if (!eval.__replaceEventReferences(ruleCopy, fact)) {
			_W("Replace failed");
		}
		ret = eval.__evaluateRuleCondition(ruleCopy, fact);
		_D("Checking condition %s", ret ? "true" : "false");
	} else {
		ret = eval.__evaluateRuleEvent(rule, fact);
		_D("Checking event %s", ret ? "true" : "false");
	}

	return ret;
}
//LCOV_EXCL_STOP
