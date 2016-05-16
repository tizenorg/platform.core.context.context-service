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

#include <sstream>
#include <context_trigger_types_internal.h>
#include <package_manager.h>
#include <Json.h>
#include "RuleManager.h"
#include "ContextMonitor.h"
#include "Rule.h"
#include "Timer.h"

#define RULE_TABLE "context_trigger_rule"

using namespace ctx;
using namespace ctx::trigger;

static int __stringToInt(std::string str)
{
	int i;
	std::istringstream convert(str);

	if (!(convert >> i))
		i = 0;

	return i;
}

static std::string __intToString(int i)
{
	std::ostringstream convert;
	convert << i;
	std::string str = convert.str();
	return str;
}

RuleManager::RuleManager()
{
}

RuleManager::~RuleManager()
{
	// Release rule instances
	_D("Release rule instances");
	for (auto it = __ruleMap.begin(); it != __ruleMap.end(); ++it) {
		Rule* rule = static_cast<Rule*>(it->second);

		int error = rule->stop();
		if (error != ERR_NONE) {
			_E("Failed to stop rule%d", it->first);
		}

		delete rule;
		__ruleMap.erase(it);
	}
}

bool RuleManager::init()
{
	bool ret;
	int error;

	// Create tables into db (rule, template)
	std::string q1 = std::string("status INTEGER DEFAULT 0 NOT NULL, creator TEXT DEFAULT '' NOT NULL,")
			+ "package_id TEXT DEFAULT '' NOT NULL, description TEXT DEFAULT '',"
			+ "details TEXT DEFAULT '' NOT NULL";
	ret = __dbManager.createTableSync(RULE_TABLE, q1.c_str(), NULL);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Create rule table failed");

	// Before re-enable rules, handle uninstalled app's rules
	if (__getUninstalledApp() > 0) {
		error = __clearRuleOfUninstalledPackage(true);
		IF_FAIL_RETURN_TAG(error == ERR_NONE, false, _E, "Failed to remove uninstalled apps' rules while initialization");
	}
	ret = __reenableRule();

	return ret;
}

void RuleManager::handleRuleOfUninstalledPackage(std::string pkgId)
{
	__uninstalledPackages.insert(pkgId);
	__clearRuleOfUninstalledPackage();
}

int RuleManager::__getUninstalledApp(void)
{
	// Return number of uninstalled apps
	std::string q1 = "SELECT DISTINCT package_id FROM context_trigger_rule";

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q1.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, -1, _E, "Query package ids of registered rules failed");

	std::vector<Json>::iterator vecEnd = record.end();
	for (std::vector<Json>::iterator vecPos = record.begin(); vecPos != vecEnd; ++vecPos) {
		Json elem = *vecPos;
		std::string pkgId;
		elem.get(NULL, "package_id", &pkgId);

		if (isUninstalledPackage(pkgId)) {
			__uninstalledPackages.insert(pkgId);
		}
	}

	return __uninstalledPackages.size();
}

bool RuleManager::isUninstalledPackage(std::string pkgId)
{
	IF_FAIL_RETURN_TAG(!pkgId.empty(), false, _D, "Empty package id");

	package_info_h pkgInfo;
	int error = package_manager_get_package_info(pkgId.c_str(), &pkgInfo);

	if (error == PACKAGE_MANAGER_ERROR_NONE) {
		package_info_destroy(pkgInfo);
	} else if (error == PACKAGE_MANAGER_ERROR_NO_SUCH_PACKAGE) {
		// Uninstalled package found
		_D("Uninstalled package found: %s", pkgId.c_str());
		return true;
	} else {
		_E("Failed to get package info(%s): %d", pkgId.c_str(), error);
	}

	return false;
}

int RuleManager::__clearRuleOfUninstalledPackage(bool isInit)
{
	if (__uninstalledPackages.size() <= 0) {
		return ERR_NONE;
	}

	int error;
	bool ret;

	_D("Clear uninstalled packages' rule started");
	// Package list
	std::string pkgList = "(";
	std::set<std::string>::iterator it = __uninstalledPackages.begin();
	pkgList += "package_id = '" + *it + "'";
	it++;
	for (; it != __uninstalledPackages.end(); ++it) {
		pkgList += " OR package_id = '" + *it + "'";
	}
	pkgList += ")";

	// After event received, disable all the enabled rules of uninstalled apps
	if (!isInit) {
		std::string q1 = "SELECT row_id FROM context_trigger_rule WHERE status = 2 and (";
		q1 += pkgList;
		q1 += ")";

		std::vector<Json> record;
		ret = __dbManager.executeSync(q1.c_str(), &record);
		IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Failed to query enabled rules of uninstalled packages");

		std::vector<Json>::iterator vecEnd = record.end();
		for (std::vector<Json>::iterator vecPos = record.begin(); vecPos != vecEnd; ++vecPos) {
			Json elem = *vecPos;
			int ruleId;
			elem.get(NULL, "row_id", &ruleId);
			error = disableRule(ruleId);
			IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to disable rule");
		}
		_D("Uninstalled packages' rules are disabled");
	}

	// Delete rules of uninstalled packages from DB
	std::string q2 = "DELETE FROM context_trigger_rule WHERE " + pkgList;
	std::vector<Json> dummy;
	ret = __dbManager.executeSync(q2.c_str(), &dummy);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Failed to remove rules from db");
	_D("Uninstalled packages' rules are deleted from db");

	__uninstalledPackages.clear();

	return ERR_NONE;
}

int RuleManager::pauseRuleWithItem(std::string& subject)
{
	std::string q = "SELECT row_id FROM context_trigger_rule WHERE (status=2) AND (details LIKE '%\"ITEM_NAME\":\"" + subject + "\"%');";
	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Failed to query row_ids to be paused");
	IF_FAIL_RETURN(record.size() > 0, ERR_NONE);

	_D("Pause rules related to %s", subject.c_str());
	std::vector<Json>::iterator vecEnd = record.end();
	for (std::vector<Json>::iterator vecPos = record.begin(); vecPos != vecEnd; ++vecPos) {
		Json elem = *vecPos;
		int rowId;
		elem.get(NULL, "row_id", &rowId);

		int error = pauseRule(rowId);
		IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to disable rules using custom item");
	}

	return ERR_NONE;
}

int RuleManager::resumeRuleWithItem(std::string& subject)
{
	std::string q = "SELECT row_id FROM context_trigger_rule WHERE (status=1) AND (details LIKE '%\"ITEM_NAME\":\"" + subject + "\"%');";
	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query paused rule ids failed");
	IF_FAIL_RETURN(record.size() > 0, ERR_NONE);

	_D("Resume rules related to %s", subject.c_str());
	std::string qRowId;
	std::vector<Json>::iterator vecEnd = record.end();
	for (std::vector<Json>::iterator vecPos = record.begin(); vecPos != vecEnd; ++vecPos) {
		Json elem = *vecPos;
		int rowId;
		elem.get(NULL, "row_id", &rowId);

		int error = enableRule(rowId);
		IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to resume rule");
	}

	return ERR_NONE;
}

bool RuleManager::__reenableRule(void)
{
	int error;
	std::string q = "SELECT row_id FROM context_trigger_rule WHERE status = 2";

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query row_ids of enabled rules failed");
	IF_FAIL_RETURN_TAG(record.size() > 0, true, _D, "No rule to re-enable");

	_D(YELLOW("Re-enable rule started"));

	std::string qRowId;
	qRowId.clear();
	std::vector<Json>::iterator vecEnd = record.end();
	for (std::vector<Json>::iterator vecPos = record.begin(); vecPos != vecEnd; ++vecPos) {
		Json elem = *vecPos;
		int rowId;
		elem.get(NULL, "row_id", &rowId);

		error = enableRule(rowId);
		if (error == ERR_NOT_SUPPORTED) {
			qRowId += "(row_id = " + __intToString(rowId) + ") OR ";
		} else if (error != ERR_NONE) {
			_E("Re-enable rule%d failed(%d)", rowId, error);
		}
	}
	IF_FAIL_RETURN(!qRowId.empty(), true);
	qRowId = qRowId.substr(0, qRowId.length() - 4);

	// For rules which is failed to re-enable
	std::string qUpdate = "UPDATE context_trigger_rule SET status = 1 WHERE " + qRowId;
	std::vector<Json> record2;
	ret = __dbManager.executeSync(qUpdate.c_str(), &record2);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Failed to update rules as paused");

	return true;
}

bool RuleManager::__ruleDataArrElemEquals(Json& lElem, Json& rElem)
{
	std::string lKey, rKey;
	lElem.get(NULL, CT_RULE_DATA_KEY, &lKey);
	rElem.get(NULL, CT_RULE_DATA_KEY, &rKey);
	if (lKey.compare(rKey))
		return false;

	int lValCnt, rValCnt, lValOpCnt, rValOpCnt;
	lValCnt = lElem.getSize(NULL, CT_RULE_DATA_VALUE_ARR);
	rValCnt = rElem.getSize(NULL, CT_RULE_DATA_VALUE_ARR);
	lValOpCnt = lElem.getSize(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR);
	rValOpCnt = rElem.getSize(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR);
	if (!((lValCnt == rValCnt) && (lValCnt == lValOpCnt) && (lValCnt && rValOpCnt)))
		return false;

	if (lValCnt > 1) {
		std::string lOp, rOp;
		lElem.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &lOp);
		rElem.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &rOp);
		if (lOp.compare(rOp))
			return false;
	}

	for (int i = 0; i < lValCnt; i++) {
		bool found = false;
		std::string lVal, lValOp;
		lElem.getAt(NULL, CT_RULE_DATA_VALUE_ARR, i, &lVal);
		lElem.getAt(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, i, &lValOp);

		for (int j = 0; j < lValCnt; j++) {
			std::string rVal, rValOp;
			rElem.getAt(NULL, CT_RULE_DATA_VALUE_ARR, j, &rVal);
			rElem.getAt(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, j, &rValOp);

			if (!lVal.compare(rVal) && !lValOp.compare(rValOp)) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	return true;
}

bool RuleManager::__ruleItemEquals(Json& lItem, Json& rItem)
{
	// Compare item name
	std::string lItemName, rItemName;
	lItem.get(NULL, CT_RULE_EVENT_ITEM, &lItemName);
	rItem.get(NULL, CT_RULE_EVENT_ITEM, &rItemName);
	if (lItemName.compare(rItemName))
		return false;

	// Compare option
	Json lOption, rOption;
	lItem.get(NULL, CT_RULE_EVENT_OPTION, &lOption);
	rItem.get(NULL, CT_RULE_EVENT_OPTION, &rOption);
	if (lOption != rOption)
		return false;

	int lDataArrCnt, rDataArrCnt;
	lDataArrCnt = lItem.getSize(NULL, CT_RULE_DATA_ARR);
	rDataArrCnt = rItem.getSize(NULL, CT_RULE_DATA_ARR);
	if (lDataArrCnt != rDataArrCnt)
		return false;

	// Compare item operator;
	if (lDataArrCnt > 1) {
		std::string lOp, rOp;
		lItem.get(NULL, CT_RULE_EVENT_OPERATOR, &lOp);
		rItem.get(NULL, CT_RULE_EVENT_OPERATOR, &rOp);
		if (lOp.compare(rOp))
			return false;
	}

	for (int i = 0; i < lDataArrCnt; i++) {
		bool found = false;
		Json lElem;
		lItem.getAt(NULL, CT_RULE_DATA_ARR, i, &lElem);

		for (int j = 0; j < lDataArrCnt; j++) {
			Json rElem;
			rItem.getAt(NULL, CT_RULE_DATA_ARR, j, &rElem);

			if (__ruleDataArrElemEquals(lElem, rElem)) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	return true;
}

bool RuleManager::__ruleEquals(Json& lRule, Json& rRule)
{
	// Compare event
	Json lEvent, rEvent;
	lRule.get(NULL, CT_RULE_EVENT, &lEvent);
	rRule.get(NULL, CT_RULE_EVENT, &rEvent);
	if (!__ruleItemEquals(lEvent, rEvent))
		return false;

	// Compare conditions
	int lCondCnt, rCondCnt;
	lCondCnt = lRule.getSize(NULL, CT_RULE_CONDITION);
	rCondCnt = rRule.getSize(NULL, CT_RULE_CONDITION);
	if (lCondCnt != rCondCnt)
		return false;

	if (lCondCnt > 1) {
		std::string lOp, rOp;
		lRule.get(NULL, CT_RULE_OPERATOR, &lOp);
		rRule.get(NULL, CT_RULE_OPERATOR, &rOp);
		if (lOp.compare(rOp))
			return false;
	}

	for (int i = 0; i < lCondCnt; i++) {
		bool found = false;
		Json lCond;
		lRule.getAt(NULL, CT_RULE_CONDITION, i, &lCond);

		for (int j = 0; j < lCondCnt; j++) {
			Json rCond;
			rRule.getAt(NULL, CT_RULE_CONDITION, j, &rCond);

			if (__ruleItemEquals(lCond, rCond)) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	// Compare action
	Json lAction, rAction;
	lRule.get(NULL, CT_RULE_ACTION, &lAction);
	rRule.get(NULL, CT_RULE_ACTION, &rAction);
	if (lAction != rAction)
		return false;

	return true;
}

int64_t RuleManager::__getDuplicatedRuleId(std::string pkgId, Json& rule)
{
	std::string q = "SELECT row_id, description, details FROM context_trigger_rule WHERE package_id = '";
	q += pkgId;
	q += "'";

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query row_id, details by package id failed");

	Json rDetails;
	rule.get(NULL, CT_RULE_DETAILS, &rDetails);
	std::string rDesc;
	rule.get(NULL, CT_RULE_DESCRIPTION, &rDesc);
	std::vector<Json>::iterator vecEnd = record.end();

	for (std::vector<Json>::iterator vecPos = record.begin(); vecPos != vecEnd; ++vecPos) {
		Json elem = *vecPos;
		std::string dStr;
		Json details;

		elem.get(NULL, "details", &dStr);
		details = dStr;

		if (__ruleEquals(rDetails, details)) {
			int64_t rowId;
			elem.get(NULL, "row_id", &rowId);

			// Description comparison
			std::string desc;
			elem.get(NULL, "description", &desc);
			if (rDesc.compare(desc)) {
				// Only description is changed
				std::string qUpdate = "UPDATE context_trigger_rule SET description='" + rDesc + "' WHERE row_id = " + __intToString(rowId);

				std::vector<Json> dummy;
				ret = __dbManager.executeSync(qUpdate.c_str(), &dummy);
				if (ret) {
					_D("Rule%lld description is updated", rowId);
				} else {
					_W("Failed to update description of rule%lld", rowId);
				}
			}

			return rowId;
		}
	}

	return -1;
}

int RuleManager::__verifyRule(Json& rule, const char* creator)
{
	Json details;
	rule.get(NULL, CT_RULE_DETAILS, &details);

	std::string eventName;
	rule.get(CT_RULE_DETAILS "." CT_RULE_EVENT, CT_RULE_EVENT_ITEM, &eventName);

	ContextMonitor* ctxMonitor = ContextMonitor::getInstance();
	IF_FAIL_RETURN_TAG(ctxMonitor, ERR_OUT_OF_MEMORY, _E, "Memory allocation failed");

	IF_FAIL_RETURN_TAG(ctxMonitor->isSupported(eventName), ERR_NOT_SUPPORTED, _I, "Event(%s) is not supported", eventName.c_str());

	if (creator) {
		if (!ctxMonitor->isAllowed(creator, eventName.c_str())) {
			_W("Permission denied for '%s'", eventName.c_str());
			return ERR_PERMISSION_DENIED;
		}
	}

	Json it;
	for (int i = 0; rule.getAt(CT_RULE_DETAILS, CT_RULE_CONDITION, i, &it); i++) {
		std::string condName;
		it.get(NULL, CT_RULE_CONDITION_ITEM, &condName);

		IF_FAIL_RETURN_TAG(ctxMonitor->isSupported(condName), ERR_NOT_SUPPORTED, _I, "Condition(%s) is not supported", condName.c_str());

		if (!ctxMonitor->isAllowed(creator, condName.c_str())) {
			_W("Permission denied for '%s'", condName.c_str());
			return ERR_PERMISSION_DENIED;
		}
	}

	return ERR_NONE;
}

int RuleManager::addRule(std::string creator, const char* pkgId, Json rule, Json* ruleId)
{
	bool ret;
	int64_t rid;

	// Check if all items are supported && allowed to access
	int err = __verifyRule(rule, creator.c_str());
	IF_FAIL_RETURN(err == ERR_NONE, err);

	// Check if duplicated rule exits
	if ((rid = __getDuplicatedRuleId(pkgId, rule)) > 0) {
		// Save rule id
		ruleId->set(NULL, CT_RULE_ID, rid);
		_D("Duplicated rule found");
		return ERR_NONE;
	}

	// Insert rule to rule table, get rule id
	Json record;
	std::string description;
	Json details;
	rule.get(NULL, CT_RULE_DESCRIPTION, &description);
	rule.get(NULL, CT_RULE_DETAILS, &details);
	record.set(NULL, "creator", creator);
	if (pkgId) {
		record.set(NULL, "package_id", pkgId);
	}
	record.set(NULL, "description", description);

	// Handle timer event
	trigger::timer::handleTimerEvent(details);

	record.set(NULL, "details", details.str());
	ret = __dbManager.insertSync(RULE_TABLE, record, &rid);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Insert rule to db failed");

	// Save rule id
	ruleId->set(NULL, CT_RULE_ID, rid);

	_D("Add rule%d succeeded", (int)rid);
	return ERR_NONE;
}


int RuleManager::removeRule(int ruleId)
{
	bool ret;

	// Delete rule from DB
	std::string query = "DELETE FROM 'context_trigger_rule' where row_id = ";
	query += __intToString(ruleId);

	std::vector<Json> record;
	ret = __dbManager.executeSync(query.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Remove rule from db failed");

	_D("Remove rule%d succeeded", ruleId);

	return ERR_NONE;
}

int RuleManager::enableRule(int ruleId)
{
	int error;
	std::string query;
	std::vector<Json> record;
	std::vector<Json> dummy;
	std::string pkgId;
	Json jRule;
	std::string tmp;
	std::string idStr = __intToString(ruleId);

	Rule* rule;

	// Get rule Json by rule id;
	query = "SELECT details, package_id FROM context_trigger_rule WHERE row_id = ";
	query += idStr;
	error = (__dbManager.executeSync(query.c_str(), &record))? ERR_NONE : ERR_OPERATION_FAILED;
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Query rule by rule id failed");

	record[0].get(NULL, "details", &tmp);
	jRule = tmp;
	record[0].get(NULL, "package_id", &pkgId);

	// Create a rule instance
	rule = new(std::nothrow) Rule(ruleId, jRule, pkgId.c_str(), this);
	IF_FAIL_RETURN_TAG(rule, ERR_OUT_OF_MEMORY, _E, "Failed to create rule instance");

	// Start the rule
	error = rule->start();
	IF_FAIL_CATCH_TAG(error == ERR_NONE, _E, "Failed to start rule%d", ruleId);

	// Update db to set 'enabled'
	query = "UPDATE context_trigger_rule SET status = 2 WHERE row_id = ";
	query += idStr;
	error = (__dbManager.executeSync(query.c_str(), &dummy))? ERR_NONE : ERR_OPERATION_FAILED;
	IF_FAIL_CATCH_TAG(error == ERR_NONE, _E, "Update db failed");

	// Add rule instance to __ruleMap
	__ruleMap[ruleId] = rule;

	_D(YELLOW("Enable Rule%d succeeded"), ruleId);
	return ERR_NONE;

CATCH:
	delete rule;
	rule = NULL;

	return error;
}

int RuleManager::disableRule(int ruleId)
{
	bool ret;
	int error;

	auto it = __ruleMap.find(ruleId);
	bool paused = (it == __ruleMap.end());

	// For 'enabled' rule, not 'paused'
	if (!paused) {
		// Stop the rule
		Rule* rule = static_cast<Rule*>(it->second);
		error = rule->stop();
		IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to stop rule%d", ruleId);

		// Remove rule instance from __ruleMap
		delete rule;
		__ruleMap.erase(it);
	}

	// Update db to set 'disabled'	// TODO skip while clear uninstalled rule
	std::string query = "UPDATE context_trigger_rule SET status = 0 WHERE row_id = ";
	query += __intToString(ruleId);
	std::vector<Json> record;
	ret = __dbManager.executeSync(query.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Update db failed");

	_D(YELLOW("Disable Rule%d succeeded"), ruleId);
	return ERR_NONE;
}

int RuleManager::pauseRule(int ruleId)
{
	bool ret;
	int error;

	auto it = __ruleMap.find(ruleId);
	IF_FAIL_RETURN_TAG(it != __ruleMap.end(), ERR_OPERATION_FAILED, _E, "Rule instance not found");

	// Stop the rule
	Rule* rule = static_cast<Rule*>(it->second);
	error = rule->stop();
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to stop rule%d", ruleId);

	// Update db to set 'paused'
	std::string query = "UPDATE context_trigger_rule SET status = 1 WHERE row_id = ";

	query += __intToString(ruleId);
	std::vector<Json> record;
	ret = __dbManager.executeSync(query.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Update db failed");

	// Remove rule instance from __ruleMap
	delete rule;
	__ruleMap.erase(it);

	_D(YELLOW("Pause Rule%d"), ruleId);
	return ERR_NONE;
}

int RuleManager::checkRule(std::string pkgId, int ruleId)
{
	// Get package id
	std::string q = "SELECT package_id FROM context_trigger_rule WHERE row_id =";
	q += __intToString(ruleId);

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query package id by rule id failed");

	if (record.size() == 0) {
		return ERR_NO_DATA;
	}

	std::string p;
	record[0].get(NULL, "package_id", &p);

	if (p.compare(pkgId) == 0) {
		return ERR_NONE;
	}

	return ERR_NO_DATA;
}

bool RuleManager::isRuleEnabled(int ruleId)
{
	std::string q = "SELECT status FROM context_trigger_rule WHERE row_id =";
	q += __intToString(ruleId);

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query enabled by rule id failed");

	int status;
	record[0].get(NULL, "status", &status);

	return (status != 0);
}

int RuleManager::getRuleById(std::string pkgId, int ruleId, Json* requestResult)
{
	std::string q = "SELECT description FROM context_trigger_rule WHERE (package_id = '";
	q += pkgId;
	q += "') and (row_id = ";
	q += __intToString(ruleId);
	q += ")";

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query rule by rule id failed");

	if (record.size() == 0) {
		return ERR_NO_DATA;
	} else if (record.size() != 1) {
		return ERR_OPERATION_FAILED;
	}

	std::string description;
	record[0].get(NULL, "description", &description);

	(*requestResult).set(NULL, CT_RULE_ID, ruleId);
	(*requestResult).set(NULL, CT_RULE_DESCRIPTION, description);

	return ERR_NONE;
}

int RuleManager::getRuleIds(std::string pkgId, Json* requestResult)
{
	(*requestResult) = "{ \"" CT_RULE_ARRAY_ENABLED "\" : [ ] , \"" CT_RULE_ARRAY_DISABLED "\" : [ ] }";

	std::string q = "SELECT row_id, status FROM context_trigger_rule WHERE (package_id = '";
	q += pkgId;
	q += "')";

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query rules failed");

	std::vector<Json>::iterator vecEnd = record.end();
	for (std::vector<Json>::iterator vecPos = record.begin(); vecPos != vecEnd; ++vecPos) {
		Json elem = *vecPos;
		std::string id;
		int status;

		elem.get(NULL, "row_id", &id);
		elem.get(NULL, "status", &status);

		if (status >= 1) {
			(*requestResult).append(NULL, CT_RULE_ARRAY_ENABLED, __stringToInt(id));
		} else if (status == 0) {
			(*requestResult).append(NULL, CT_RULE_ARRAY_DISABLED, __stringToInt(id));
		}
	}

	return ERR_NONE;
}
