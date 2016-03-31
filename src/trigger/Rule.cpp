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

#include <context_trigger_types_internal.h>
#include "Rule.h"
#include "ActionManager.h"
#include "RuleEvaluator.h"
#include "ContextMonitor.h"
#include "FactTypes.h"
#include "RuleManager.h"

using namespace ctx;
using namespace ctx::trigger;

RuleManager *Rule::__ruleMgr = NULL;

Rule::Rule(int i, Json& d, const char* p, RuleManager* rm) :
	 __result(EMPTY_JSON_OBJECT),
	id(i),
	pkgId(p)
{
	// Rule manager
	if (!__ruleMgr) {
		__ruleMgr = rm;
	}

	// Statement
	__statement = d.str();

	// Event
	Json e;
	d.get(NULL, CT_RULE_EVENT, &e);
	__event = new(std::nothrow) ContextItem(e);

	// Condition
	int cond_num = d.getSize(NULL, CT_RULE_CONDITION);
	for (int j = 0; j < cond_num; j++) {
		Json c;
		d.getAt(NULL, CT_RULE_CONDITION, j, &c);
		__condition.push_back(new(std::nothrow) ContextItem(c));
	}

	// Action
	Json a;
	d.get(NULL, CT_RULE_ACTION, &a);
	__action = a.str();
}

Rule::~Rule()
{
	// Release resources
	delete __event;
	for (auto it = __condition.begin(); it != __condition.end(); ++it) {
		delete *it;
	}
}

int Rule::start(void)
{
	// Subscribe event
	int error = ContextMonitor::getInstance()->subscribe(id, __event->name, __event->option, this);
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to start rule%d", id);

	return error;
}

int Rule::stop(void)
{
	// Unsubscribe event
	int error = ContextMonitor::getInstance()->unsubscribe(id, __event->name, __event->option, this);
	if (error == ERR_NOT_SUPPORTED) {
		_E("Stop rule%d (event not supported)");
		return ERR_NONE;
	}
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to stop rule%d", id);

	return error;
}

bool Rule::__setConditionOptionBasedOnEvent(Json& option)
{
	// Set condition option if it references event data
	std::list<std::string> optionKeys;
	option.getKeys(&optionKeys);

	for (auto it = optionKeys.begin(); it != optionKeys.end(); ++it) {
		std::string optKey = (*it);

		std::string optVal;
		if (option.get(NULL, optKey.c_str(), &optVal)) {
			if (optVal.find("?") != 0) {
				continue;
			}

			std::string eventKey = optVal.substr(1, optVal.length() - 1);

			std::string newStr;
			int new_val;
			if (__result.get(CONTEXT_FACT_EVENT "." CONTEXT_FACT_DATA, eventKey.c_str(), &newStr)) {
				option.set(NULL, optKey.c_str(), newStr);
			} else if (__result.get(CONTEXT_FACT_EVENT "." CONTEXT_FACT_DATA, eventKey.c_str(), &new_val)) {
				option.set(NULL, optKey.c_str(), new_val);
			} else {
				_W("Failed to find '%s' in event data", eventKey.c_str());
				return false;
			}
		}
	}

	return true;
}

void Rule::onEventReceived(std::string name, Json option, Json data)
{
	if (__result != EMPTY_JSON_OBJECT) {
		__clearResult();
	}

	// Check if creator package is uninstalled
	if (RuleManager::isUninstalledPackage(pkgId)) {
		_D("Creator(%s) of rule%d is uninstalled.", pkgId.c_str(), id);
		g_idle_add(__handleUninstalledRule, &pkgId);
		return;
	}

	_D("Rule%d received event data", id);

	// Set event data
	__result.set(CONTEXT_FACT_EVENT, CONTEXT_FACT_NAME, name);
	__result.set(CONTEXT_FACT_EVENT, CONTEXT_FACT_OPTION, option);
	__result.set(CONTEXT_FACT_EVENT, CONTEXT_FACT_DATA, data);

	if (__condition.size() == 0) {
		__onContextDataPrepared();
		return;
	}

	IF_FAIL_VOID_TAG(RuleEvaluator::evaluateRule(__statement, __result), _E, "Event not matched");

	// Request read conditions
	for (auto it = __condition.begin(); it != __condition.end(); ++it) {
		Json condOption = (*it)->option.str();
		if (!__setConditionOptionBasedOnEvent(condOption)) { // condOption should be copy of original option.
			__clearResult();
			return;
		}

		int error = ContextMonitor::getInstance()->read((*it)->name.c_str(), condOption, this);
		IF_FAIL_VOID_TAG(error == ERR_NONE, _E, "Failed to read condition");
	}

	// TODO timer set
}

void Rule::onConditionReceived(std::string name, Json option, Json data)
{
	_D("Rule%d received condition data", id);

	// Set condition data
	Json item;
	item.set(NULL, CONTEXT_FACT_NAME, name);
	item.set(NULL, CONTEXT_FACT_OPTION, option);
	item.set(NULL, CONTEXT_FACT_DATA, data);
	__result.append(NULL, CONTEXT_FACT_CONDITION, item);

	if (__result.getSize(NULL, CONTEXT_FACT_CONDITION) == (int) __condition.size()) {
		__onContextDataPrepared();
	}
}

void Rule::__clearResult()
{
	__result = EMPTY_JSON_OBJECT;
	// TODO timer cancel
}

void Rule::__onContextDataPrepared(void)
{
	if (RuleEvaluator::evaluateRule(__statement, __result)) {
		action_manager::triggerAction(__action, pkgId);
	}
	__clearResult();
}

gboolean Rule::__handleUninstalledRule(gpointer data)
{
	std::string* pkgId = static_cast<std::string*>(data);
	__ruleMgr->handleRuleOfUninstalledPackage(*pkgId);

	return FALSE;
}
