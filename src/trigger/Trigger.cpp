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

#include <Types.h>
#include <context_trigger_types_internal.h>
#include "Trigger.h"
#include "ContextMonitor.h"
#include "TemplateManager.h"
#include "RuleManager.h"

using namespace ctx;
using namespace ctx::trigger;

Trigger::Trigger() :
	__ruleMgr(NULL)
{
}

Trigger::~Trigger()
{
}

bool Trigger::init(ContextManager* ctxMgr)
{
	// Do the necessary initialization process.
	// This function is called from the main thread during the service launching process.
	_D("Context Trigger Init");
	__processInitialize(ctxMgr);

	return true;
}

void Trigger::release()
{
	// Release the occupied resources.
	// This function is called from the main thread during the service termination process.

	_D("Template Manager Destroy");
	TemplateManager::destroy();

	_D("Rule Manager Release");
	delete __ruleMgr;
	__ruleMgr = NULL;

	_D("Context Monitor Destroy");
	ContextMonitor::destroy();
}

bool Trigger::assignRequest(RequestInfo* request)
{
	std::string subject = request->getSubject();
	if (subject != CONTEXT_TRIGGER_SUBJECT_ADD && subject != CONTEXT_TRIGGER_SUBJECT_REMOVE &&
			subject != CONTEXT_TRIGGER_SUBJECT_ENABLE && subject != CONTEXT_TRIGGER_SUBJECT_DISABLE	&&
			subject != CONTEXT_TRIGGER_SUBJECT_GET && subject != CONTEXT_TRIGGER_SUBJECT_GET_RULE_IDS &&
			subject != CONTEXT_TRIGGER_SUBJECT_GET_TEMPLATE) {
		return false;
	}

	__processRequest(request);
	return true;
}

void Trigger::__processRequest(RequestInfo* request)
{
	// Process the request, and reply to the client if necessary.
	const char* reqSubj = request->getSubject();
	_D("Request is %s", reqSubj);
	std::string subject(reqSubj);

	if (subject == CONTEXT_TRIGGER_SUBJECT_ADD) {
		__addRule(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_REMOVE) {
		__removeRule(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_ENABLE) {
		__enableRule(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_DISABLE) {
		__disableRule(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_GET) {
		__getRuleById(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_GET_RULE_IDS) {
		__getRuleIds(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_GET_TEMPLATE) {
		__getTemplate(request);
	} else {
		_E("Invalid request");
	}

	delete request;
}

void Trigger::__processInitialize(ContextManager* mgr)
{
	// Context Monitor
	ContextMonitor::setContextManager(mgr);

	// Rule Manager
	__ruleMgr = new(std::nothrow) RuleManager();
	IF_FAIL_VOID_TAG(__ruleMgr, _E, "Memory allocation failed");

	// Template Manager
	TemplateManager::setManager(mgr, __ruleMgr);
	TemplateManager* tmplMgr = TemplateManager::getInstance();
	IF_FAIL_VOID_TAG(tmplMgr, _E, "Memory allocation failed");

	// Initialization
	if (!tmplMgr->init()) {
		_E("Template manager initialization failed");
		raise(SIGTERM);
	}

	if (!__ruleMgr->init()) {
		_E("Context trigger initialization failed");
		raise(SIGTERM);
	}
}

void Trigger::__addRule(RequestInfo* request)
{
	Json ruleId;

	const char* client = request->getClient();
	if (client == NULL) {
		request->reply(ERR_OPERATION_FAILED);
		return;
	}

	const char* pkgId = request->getPackageId();

	int error = __ruleMgr->addRule(client, pkgId, request->getDescription(), &ruleId);
	_I("'%s' adds a rule (Error: %#x)", request->getClient(), error);

	request->reply(error, ruleId);
}

void Trigger::__removeRule(RequestInfo* request)
{
	int id;
	int error;

	const char* pkgId = request->getPackageId();

	Json ruleId = request->getDescription();
	ruleId.get(NULL, CT_RULE_ID, &id);

	error = __ruleMgr->checkRule((pkgId)? pkgId : "", id);
	if (error != ERR_NONE) {
		request->reply(error);
		return;
	}

	bool ret = __ruleMgr->isRuleEnabled(id);
	if (ret) {
		request->reply(ERR_RULE_ENABLED);
		return;
	}

	error = __ruleMgr->removeRule(id);
	_I("'%s' removes rule%d (Error: %#x)", request->getClient(), id, error);
	request->reply(error);
}

void Trigger::__enableRule(RequestInfo* request)
{
	int id;
	int error;

	const char* pkgId = request->getPackageId();

	Json ruleId = request->getDescription();
	ruleId.get(NULL, CT_RULE_ID, &id);

	error = __ruleMgr->checkRule((pkgId)? pkgId : "", id);
	if (error != ERR_NONE) {
		request->reply(error);
		return;
	}

	bool ret = __ruleMgr->isRuleEnabled(id);
	if (ret) {
		request->reply(ERR_RULE_ENABLED);
		return;
	}

	error = __ruleMgr->enableRule(id);
	_I("'%s' enables rule%d (Error: %#x)", request->getClient(), id, error);
	request->reply(error);
}

void Trigger::__disableRule(RequestInfo* request)
{
	int id;
	int error;

	const char* pkgId = request->getPackageId();

	Json ruleId = request->getDescription();
	ruleId.get(NULL, CT_RULE_ID, &id);

	error = __ruleMgr->checkRule((pkgId)? pkgId : "", id);
	if (error != ERR_NONE) {
		request->reply(error);
		return;
	}

	bool ret = __ruleMgr->isRuleEnabled(id);
	if (!ret) {
		request->reply(ERR_RULE_NOT_ENABLED);
		return;
	}

	error = __ruleMgr->disableRule(id);
	_I("'%s' disables rule%d (Error: %#x)", request->getClient(), id, error);
	request->reply(error);
}

void Trigger::__getRuleById(RequestInfo* request)
{
	int error;

	Json option = request->getDescription();
	int id;
	option.get(NULL, CT_RULE_ID, &id);

	const char* pkgId = request->getPackageId();

	Json readData;
	error = __ruleMgr->getRuleById((pkgId)? pkgId : "", id, &readData);

	Json dummy;
	request->reply(error, dummy, readData);
}

void Trigger::__getRuleIds(RequestInfo* request)
{
	int error;

	const char* pkgId = request->getPackageId();

	Json readData;
	error = __ruleMgr->getRuleIds((pkgId)? pkgId : "", &readData);

	Json dummy;
	request->reply(error, dummy, readData);
}

void Trigger::__getTemplate(RequestInfo* request)
{
	int error;

	Json option = request->getDescription();
	std::string name;
	option.get(NULL, SUBJECT_STR, &name);

	TemplateManager* tmplMgr = TemplateManager::getInstance();
	if (!tmplMgr) {
		_E("Memory allocation failed");
		request->reply(ERR_OUT_OF_MEMORY);
		return;
	}

	Json tmpl;
	error = tmplMgr->getTemplate(name, &tmpl);

	Json dummy;
	request->reply(error, dummy, tmpl);
}
