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

#include <glib.h>
#include <list>

#include <types_internal.h>
#include <context_trigger_types_internal.h>
#include <Json.h>
#include "Server.h"
#include "access_control/Privilege.h"
#include "Request.h"
#include "ProviderHandler.h"
#include "ContextManagerImpl.h"
#include "trigger/TemplateManager.h"

/* Context Providers */
#include <internal/DeviceContextProvider.h>
#include <internal/StatisticsContextProvider.h>
#include <internal/PlaceContextProvider.h>
#include <internal/CustomContextProvider.h>

struct TriggerItemFormat {
	std::string subject;
	int operation;
	ctx::Json attributes;
	ctx::Json options;
	std::string owner;
	bool unregister;
	TriggerItemFormat(std::string subj, int ops, ctx::Json attr, ctx::Json opt, std::string own) :
		subject(subj),
		operation(ops),
		attributes(attr),
		options(opt),
		owner(own)
	{
		unregister = false;
	}

	TriggerItemFormat(std::string subj) :
		subject(subj)
	{
		unregister = true;
	}
};

static std::list<TriggerItemFormat> __triggerItemList;
bool ctx::ContextManagerImpl::__initialized = false;

ctx::ContextManagerImpl::ContextManagerImpl()
{
}

ctx::ContextManagerImpl::~ContextManagerImpl()
{
	release();
}

bool ctx::ContextManagerImpl::init()
{
	bool ret;

	ret = initDeviceContextProvider();
	IF_FAIL_RETURN_TAG(ret, false, _E, "Initialization failed: DeviceContextProvider");

	ret = initStatisticsContextProvider();
	IF_FAIL_RETURN_TAG(ret, false, _E, "Initialization failed: StatisticsContextProvider");

	ret = initPlaceContextProvider();
	IF_FAIL_RETURN_TAG(ret, false, _E, "Initialization failed: PlaceContextProvider");

	ret = initCustomContextProvider();
	IF_FAIL_RETURN_TAG(ret, false, _E, "Initialization failed: CustomContextProvider");

	__initialized = true;

	return true;
}

void ctx::ContextManagerImpl::release()
{
	for (auto it = __providerHandleMap.begin(); it != __providerHandleMap.end(); ++it) {
		delete it->second;
	}
	__providerHandleMap.clear();
}

bool ctx::ContextManagerImpl::registerProvider(const char *subject, ctx::ContextProviderInfo &providerInfo)
{
	if (__providerHandleMap.find(subject) != __providerHandleMap.end()) {
		_E("The provider for the subject '%s' is already registered.", subject);
		return false;
	}

	_SI("Subj: %s, Priv: %s", subject, providerInfo.privilege);
	__providerHandleMap[subject] = NULL;

	auto it = __providerHandleMap.find(subject);
	ProviderHandler *handle = new(std::nothrow) ProviderHandler(it->first.c_str(), providerInfo);

	if (!handle) {
		_E("Memory allocation failed");
		__providerHandleMap.erase(it);
		return false;
	}

	it->second = handle;
	return true;
}

bool ctx::ContextManagerImpl::unregisterProvider(const char *subject)
{
	auto it = __providerHandleMap.find(subject);
	if (it == __providerHandleMap.end()) {
		_E("The provider for the subject '%s' is not found.", subject);
		return false;
	}

	delete it->second;
	__providerHandleMap.erase(it);

	return true;
}

bool ctx::ContextManagerImpl::registerTriggerItem(const char *subject, int operation, ctx::Json attributes, ctx::Json options, const char* owner)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	if (!__initialized) {
		__triggerItemList.push_back(TriggerItemFormat(subject, operation, attributes, options, (owner)? owner : ""));
	} else {
		ctx::trigger::TemplateManager* tmplMgr = ctx::trigger::TemplateManager::getInstance();
		IF_FAIL_RETURN_TAG(tmplMgr, false, _E, "Memory allocation failed");
		tmplMgr->registerTemplate(subject, operation, attributes, options, owner);
	}

	return true;
}

bool ctx::ContextManagerImpl::unregisterTriggerItem(const char *subject)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	if (!__initialized) {
		__triggerItemList.push_back(TriggerItemFormat(subject));
	} else {
		ctx::trigger::TemplateManager* tmplMgr = ctx::trigger::TemplateManager::getInstance();
		IF_FAIL_RETURN_TAG(tmplMgr, false, _E, "Memory allocation failed");
		tmplMgr->unregisterTemplate(subject);
	}

	return true;
}

bool ctx::ContextManagerImpl::popTriggerItem(std::string &subject, int &operation, ctx::Json &attributes, ctx::Json &options, std::string& owner, bool& unregister)
{
	IF_FAIL_RETURN(!__triggerItemList.empty(), false);

	TriggerItemFormat format = __triggerItemList.front();
	__triggerItemList.pop_front();

	subject = format.subject;
	operation = format.operation;
	attributes = format.attributes;
	options = format.options;
	owner = format.owner;
	unregister = format.unregister;

	return true;
}

void ctx::ContextManagerImpl::assignRequest(ctx::RequestInfo* request)
{
	if (__handleCustomRequest(request)) {
		delete request;
		return;
	}

	auto it = __providerHandleMap.find(request->getSubject());
	if (it == __providerHandleMap.end()) {
		_W("Unsupported subject");
		request->reply(ERR_NOT_SUPPORTED);
		delete request;
		return;
	}

	if (!it->second->isAllowed(request->getCredentials())) {
		_W("Permission denied");
		request->reply(ERR_PERMISSION_DENIED);
		delete request;
		return;
	}

	switch (request->getType()) {
	case REQ_SUBSCRIBE:
		it->second->subscribe(request);
		break;
	case REQ_UNSUBSCRIBE:
		it->second->unsubscribe(request);
		break;
	case REQ_READ:
	case REQ_READ_SYNC:
		it->second->read(request);
		break;
	case REQ_WRITE:
		it->second->write(request);
		break;
	case REQ_SUPPORT:
		request->reply(ERR_NONE);
		delete request;
		break;
	default:
		_E("Invalid type of request");
		delete request;
	}
}

bool ctx::ContextManagerImpl::isSupported(const char *subject)
{
	auto it = __providerHandleMap.find(subject);
	return (it != __providerHandleMap.end());
}

bool ctx::ContextManagerImpl::isAllowed(const ctx::Credentials *creds, const char *subject)
{
	IF_FAIL_RETURN(creds, true);	/* In case internal requests */
	auto it = __providerHandleMap.find(subject);
	IF_FAIL_RETURN(it != __providerHandleMap.end(), false);
	return it->second->isAllowed(creds);
}

void ctx::ContextManagerImpl::__publish(const char* subject, ctx::Json &option, int error, ctx::Json &dataUpdated)
{
	_I("Publishing '%s'", subject);
	_J("Option", option);

	auto it = __providerHandleMap.find(subject);
	IF_FAIL_VOID(it != __providerHandleMap.end());

	it->second->publish(option, error, dataUpdated);
}

void ctx::ContextManagerImpl::__replyToRead(const char* subject, ctx::Json &option, int error, ctx::Json &dataRead)
{
	_I("Sending data of '%s'", subject);
	_J("Option", option);
	_J("Data", dataRead);

	auto it = __providerHandleMap.find(subject);
	IF_FAIL_VOID(it != __providerHandleMap.end());

	it->second->replyToRead(option, error, dataRead);
}

struct PublishedData {
	int type;
	ctx::ContextManagerImpl *mgr;
	std::string subject;
	int error;
	ctx::Json option;
	ctx::Json data;
	PublishedData(int t, ctx::ContextManagerImpl *m, const char* s, ctx::Json& o, int e, ctx::Json& d)
		: type(t), mgr(m), subject(s), error(e)
	{
		option = o.str();
		data = d.str();
	}
};

gboolean ctx::ContextManagerImpl::__threadSwitcher(gpointer data)
{
	PublishedData *tuple = static_cast<PublishedData*>(data);

	switch (tuple->type) {
	case REQ_SUBSCRIBE:
		tuple->mgr->__publish(tuple->subject.c_str(), tuple->option, tuple->error, tuple->data);
		break;
	case REQ_READ:
		tuple->mgr->__replyToRead(tuple->subject.c_str(), tuple->option, tuple->error, tuple->data);
		break;
	default:
		_W("Invalid type");
	}

	delete tuple;
	return FALSE;
}

bool ctx::ContextManagerImpl::publish(const char* subject, ctx::Json& option, int error, ctx::Json& dataUpdated)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	PublishedData *tuple = new(std::nothrow) PublishedData(REQ_SUBSCRIBE, this, subject, option, error, dataUpdated);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(__threadSwitcher, tuple);

	return true;
}

bool ctx::ContextManagerImpl::replyToRead(const char* subject, ctx::Json& option, int error, ctx::Json& dataRead)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	PublishedData *tuple = new(std::nothrow) PublishedData(REQ_READ, this, subject, option, error, dataRead);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(__threadSwitcher, tuple);

	return true;
}

bool ctx::ContextManagerImpl::__handleCustomRequest(ctx::RequestInfo* request)
{
	std::string subject = request->getSubject();
	IF_FAIL_RETURN(	subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_ADD ||
					subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_REMOVE ||
					subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_PUBLISH, false);

	const char* pkg_id = request->getPackageId();
	if (pkg_id == NULL) {
		request->reply(ERR_OPERATION_FAILED);
		return true;
	}

	ctx::Json desc = request->getDescription();
	std::string name;
	desc.get(NULL, CT_CUSTOM_NAME, &name);
	std::string subj = pkg_id + std::string("::") + name;

	int error = ERR_NONE;
	if (subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_ADD) {
		ctx::Json tmpl;
		desc.get(NULL, CT_CUSTOM_ATTRIBUTES, &tmpl);

		error = ctx::custom_context_provider::addItem(subj, name, tmpl, pkg_id);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_REMOVE) {
		error = ctx::custom_context_provider::removeItem(subj);
		if (error == ERR_NONE) {
			ctx::Json data;
			data.set(NULL, CT_CUSTOM_SUBJECT, subj);
			request->reply(error, data);
		}
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_PUBLISH) {
		ctx::Json fact;
		desc.get(NULL, CT_CUSTOM_FACT, &fact);

		error = ctx::custom_context_provider::publishData(subj, fact);
	}

	request->reply(error);
	return true;
}
