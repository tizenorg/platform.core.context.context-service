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

#include <Types.h>
#include <DBusTypes.h>
#include <Json.h>
#include <ContextProvider.h>

#include "access_control/Privilege.h"
#include "trigger/TemplateManager.h"
#include "Server.h"
#include "Request.h"
#include "ProviderHandler.h"
#include "ProviderLoader.h"
#include "ContextManager.h"

using namespace ctx;

ContextManager::ContextManager()
{
	ContextProvider::__setContextManager(this);
	ProviderLoader::init();
}

ContextManager::~ContextManager()
{
	release();
}

bool ContextManager::init()
{
	return true;
}

void ContextManager::release()
{
	ProviderHandler::purge();
}

void ContextManager::assignRequest(RequestInfo* request)
{
	ProviderHandler *handle = ProviderHandler::getInstance(request->getSubject(), true);
	if (!handle || !handle->isSupported()) {
		request->reply(ERR_NOT_SUPPORTED);
		delete request;
		return;
	}

	if (!handle->isAllowed(request->getCredentials())) {
		_W("Permission denied");
		request->reply(ERR_PERMISSION_DENIED);
		delete request;
		return;
	}

	switch (request->getType()) {
	case REQ_SUBSCRIBE:
		handle->subscribe(request);
		break;
	case REQ_UNSUBSCRIBE:
		handle->unsubscribe(request);
		break;
	case REQ_READ:
	case REQ_READ_SYNC:
		handle->read(request);
		break;
	case REQ_WRITE:
		handle->write(request);
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

bool ContextManager::isSupported(const char *subject)
{
	ProviderHandler *handle = ProviderHandler::getInstance(subject, true);

	if (!handle)
		return false;

	return handle->isSupported();
}

bool ContextManager::isAllowed(const Credentials *creds, const char *subject)
{
	IF_FAIL_RETURN(creds, true);	/* In case internal requests */

	ProviderHandler *handle = ProviderHandler::getInstance(subject, true);

	if (!handle)
		return false;

	return handle->isAllowed(creds);
}

void ContextManager::__publish(const char* subject, Json &option, int error, Json &dataUpdated)
{
	_I("Publishing '%s'", subject);
	_J("Option", option);

	ProviderHandler *handle = ProviderHandler::getInstance(subject, false);
	IF_FAIL_VOID_TAG(handle, _W, "No corresponding provider");

	handle->publish(option, error, dataUpdated);
}

void ContextManager::__replyToRead(const char* subject, Json &option, int error, Json &dataRead)
{
	_I("Sending data of '%s'", subject);
	_J("Option", option);
	_J("Data", dataRead);

	ProviderHandler *handle = ProviderHandler::getInstance(subject, false);
	IF_FAIL_VOID_TAG(handle, _W, "No corresponding provider");

	handle->replyToRead(option, error, dataRead);
}

struct PublishedData {
	int type;
	ContextManager *mgr;
	std::string subject;
	int error;
	Json option;
	Json data;
	PublishedData(int t, ContextManager *m, const char* s, Json& o, int e, Json& d)
		: type(t), mgr(m), subject(s), error(e)
	{
		option = o.str();
		data = d.str();
	}
};

gboolean ContextManager::__threadSwitcher(gpointer data)
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

bool ContextManager::publish(const char* subject, Json& option, int error, Json& dataUpdated)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	PublishedData *tuple = new(std::nothrow) PublishedData(REQ_SUBSCRIBE, this, subject, option, error, dataUpdated);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(__threadSwitcher, tuple);

	return true;
}

bool ContextManager::replyToRead(const char* subject, Json& option, int error, Json& dataRead)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	PublishedData *tuple = new(std::nothrow) PublishedData(REQ_READ, this, subject, option, error, dataRead);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(__threadSwitcher, tuple);

	return true;
}

bool ContextManager::popTriggerTemplate(std::string &subject, int &operation, Json &attribute, Json &option)
{
	return ProviderLoader::popTriggerTemplate(subject, operation, attribute, option);
}

/*
bool ContextManager::__handleCustomRequest(RequestInfo* request)
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

	Json desc = request->getDescription();
	std::string name;
	desc.get(NULL, CT_CUSTOM_NAME, &name);
	std::string subj = pkg_id + std::string("::") + name;

	int error = ERR_NONE;
	if (subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_ADD) {
		Json tmpl;
		desc.get(NULL, CT_CUSTOM_ATTRIBUTES, &tmpl);

		error = custom_context_provider::addItem(subj, name, tmpl, pkg_id);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_REMOVE) {
		error = custom_context_provider::removeItem(subj);
		if (error == ERR_NONE) {
			Json data;
			data.set(NULL, CT_CUSTOM_SUBJECT, subj);
			request->reply(error, data);
		}
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_PUBLISH) {
		Json fact;
		desc.get(NULL, CT_CUSTOM_FACT, &fact);

		error = custom_context_provider::publishData(subj, fact);
	}

	request->reply(error);
	return true;
}
*/
