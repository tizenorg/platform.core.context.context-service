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
#include <Types.h>
#include <context_trigger_types_internal.h>
#include "../ContextManager.h"
#include "RuleManager.h"
#include "TemplateManager.h"

using namespace ctx;
using namespace ctx::trigger;

TemplateManager *TemplateManager::__instance = NULL;
ContextManager *TemplateManager::__contextMgr = NULL;
RuleManager *TemplateManager::__ruleMgr = NULL;

static std::string __intToString(int i)
{
	std::ostringstream convert;
	convert << i;
	std::string str = convert.str();
	return str;
}

TemplateManager::TemplateManager()
{
}

TemplateManager::~TemplateManager()
{
}

void TemplateManager::setManager(ContextManager* ctxMgr, RuleManager* ruleMgr)
{
	__contextMgr = ctxMgr;
	__ruleMgr = ruleMgr;
}

TemplateManager* TemplateManager::getInstance()
{
	IF_FAIL_RETURN_TAG(__contextMgr, NULL, _E, "Context manager is needed");
	IF_FAIL_RETURN_TAG(__ruleMgr, NULL, _E, "Rule manager is needed");

	IF_FAIL_RETURN(!__instance, __instance);

	__instance = new(std::nothrow) TemplateManager();
	IF_FAIL_RETURN_TAG(__instance, NULL, _E, "Memory alllocation failed");

	return __instance;
}

void TemplateManager::destroy()
{
	IF_FAIL_VOID(__instance);

	__instance->applyTemplates();

	delete __instance;
	__instance = NULL;
}

bool TemplateManager::init()
{
	std::string q = std::string("CREATE TABLE IF NOT EXISTS ContextTriggerTemplate ")
			+ "(name TEXT DEFAULT '' NOT NULL PRIMARY KEY, operation INTEGER DEFAULT 3 NOT NULL, "
			+ "attributes TEXT DEFAULT '' NOT NULL, options TEXT DEFAULT '' NOT NULL, owner TEXT DEFAULT '' NOT NULL)";

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Create template table failed");

	// Apply templates
	applyTemplates();

	return true;
}

void TemplateManager::applyTemplates()
{
	std::string subject;
	int operation;
	Json attributes;
	Json options;
	std::string owner;
	std::string query;
	query.clear();

	while (__contextMgr->popTriggerTemplate(subject, operation, attributes, options)) {
		registerTemplate(subject, operation, attributes, options, "");
	}
}

void TemplateManager::registerTemplate(std::string subject, int operation, Json attributes, Json options, std::string owner)
{
	_D("[Add template] Subject: %s, Ops: %d, Owner: %s", subject.c_str(), operation, owner.c_str());
	_J("Attr", attributes);
	_J("Opt", options);

	std::string query = "UPDATE ContextTriggerTemplate SET operation=" + __intToString(operation)
			+ ", attributes='" + attributes.str() + "', options='" + options.str() + "', owner='" + owner
			+ "' WHERE name='" + subject + "'; ";

	query += "INSERT OR IGNORE INTO ContextTriggerTemplate (name, operation, attributes, options, owner) VALUES ('"
			+ subject + "', " + __intToString(operation) + ", '" + attributes.str() + "', '" + options.str() + "', '"
			+ owner + "'); ";

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(query.c_str(), &record);
	IF_FAIL_VOID_TAG(ret, _E, "Update template db failed");

	if (!owner.empty()) {
		__ruleMgr->resumeRuleWithItem(subject);
	}
}

void TemplateManager::unregisterTemplate(std::string subject)
{
	_D("[Remove template] Subject: %s", subject.c_str());
	std::string query = "DELETE FROM ContextTriggerTemplate WHERE name = '" + subject + "'; ";

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(query.c_str(), &record);
	IF_FAIL_VOID_TAG(ret, _E, "Update template db failed");

	__ruleMgr->pauseRuleWithItem(subject);
}


std::string TemplateManager::__addTemplate(std::string &subject, int &operation, Json &attributes, Json &options, std::string &owner)
{
	_D("[Add template] Subject: %s, Ops: %d, Owner: %s", subject.c_str(), operation, owner.c_str());
	_J("Attr", attributes);
	_J("Opt", options);

	std::string query = "UPDATE ContextTriggerTemplate SET operation=" + __intToString(operation)
			+ ", attributes='" + attributes.str() + "', options='" + options.str() + "', owner='" + owner
			+ "' WHERE name='" + subject + "'; ";

	query += "INSERT OR IGNORE INTO ContextTriggerTemplate (name, operation, attributes, options, owner) VALUES ('"
			+ subject + "', " + __intToString(operation) + ", '" + attributes.str() + "', '" + options.str() + "', '"
			+ owner + "'); ";

	return query;
}

std::string TemplateManager::__removeTemplate(std::string &subject)
{
	_D("[Remove template] Subject: %s", subject.c_str());
	std::string query = "DELETE FROM ContextTriggerTemplate WHERE name = '" + subject + "'; ";

	return query;
}

int TemplateManager::getTemplate(std::string &subject, Json* tmpl)
{
	if (!__contextMgr->isSupported(subject.c_str()))
		return ERR_NOT_SUPPORTED;

	// Update latest template information
	std::string q = "SELECT * FROM ContextTriggerTemplate WHERE name = '" + subject + "'";

	std::vector<Json> record;
	bool ret = __dbManager.executeSync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query template failed");
	IF_FAIL_RETURN_TAG(record.size() > 0, ERR_NOT_SUPPORTED, _E, "Template(%s) not found", subject.c_str());
	IF_FAIL_RETURN_TAG(record.size() == 1, ERR_OPERATION_FAILED, _E, "Tepmlate duplicated");

	(*tmpl) = *record.begin();

	std::string optStr;
	std::string attrStr;
	tmpl->get(NULL, TYPE_OPTION_STR, &optStr);
	tmpl->get(NULL, TYPE_ATTR_STR, &attrStr);

	Json opt = optStr;
	Json attr = attrStr;

	tmpl->set(NULL, TYPE_OPTION_STR, opt);
	tmpl->set(NULL, TYPE_ATTR_STR, attr);

	return ERR_NONE;
}
