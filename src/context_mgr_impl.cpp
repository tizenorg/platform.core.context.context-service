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
#include <provider_iface.h>
#include "server.h"
#include "access_control/Privilege.h"
#include "Request.h"
#include "provider.h"
#include "context_mgr_impl.h"
#include "trigger/TemplateManager.h"

/* Context Providers */
#include <internal/device_context_provider.h>
#include <internal/statistics_context_provider.h>
#include <internal/place_context_provider.h>
#include <internal/custom_context_provider.h>

struct trigger_item_format_s {
	std::string subject;
	int operation;
	ctx::Json attributes;
	ctx::Json options;
	std::string owner;
	bool unregister;
	trigger_item_format_s(std::string subj, int ops, ctx::Json attr, ctx::Json opt, std::string own)
		: subject(subj), operation(ops), attributes(attr), options(opt), owner(own)
	{
		unregister = false;
	}

	trigger_item_format_s(std::string subj)
		: subject(subj) {
		unregister = true;
	}
};

static std::list<trigger_item_format_s> __trigger_item_list;
bool ctx::context_manager_impl::initialized = false;

ctx::context_manager_impl::context_manager_impl()
{
}

ctx::context_manager_impl::~context_manager_impl()
{
	release();
}

bool ctx::context_manager_impl::init()
{
	bool ret;

	ret = init_device_context_provider();
	IF_FAIL_RETURN_TAG(ret, false, _E, "Initialization failed: device-context-provider");

	ret = init_statistics_context_provider();
	IF_FAIL_RETURN_TAG(ret, false, _E, "Initialization failed: statistics-context-provider");

	ret = init_place_context_provider();
	IF_FAIL_RETURN_TAG(ret, false, _E, "Initialization failed: place-context-provider");

	ret = init_custom_context_provider();
	IF_FAIL_RETURN_TAG(ret, false, _E, "Initialization failed: custom-context-provider");

	initialized = true;

	return true;
}

void ctx::context_manager_impl::release()
{
	for (auto it = provider_handle_map.begin(); it != provider_handle_map.end(); ++it) {
		delete it->second;
	}
	provider_handle_map.clear();
}

bool ctx::context_manager_impl::register_provider(const char *subject, ctx::context_provider_info &provider_info)
{
	if (provider_handle_map.find(subject) != provider_handle_map.end()) {
		_E("The provider for the subject '%s' is already registered.", subject);
		return false;
	}

	_SI("Subj: %s, Priv: %s", subject, provider_info.privilege);
	provider_handle_map[subject] = NULL;

	auto it = provider_handle_map.find(subject);
	context_provider_handler *handle = new(std::nothrow) context_provider_handler(it->first.c_str(), provider_info);

	if (!handle) {
		_E("Memory allocation failed");
		provider_handle_map.erase(it);
		return false;
	}

	it->second = handle;
	return true;
}

bool ctx::context_manager_impl::unregister_provider(const char *subject)
{
	auto it = provider_handle_map.find(subject);
	if (it == provider_handle_map.end()) {
		_E("The provider for the subject '%s' is not found.", subject);
		return false;
	}

	delete it->second;
	provider_handle_map.erase(it);

	return true;
}

bool ctx::context_manager_impl::register_trigger_item(const char *subject, int operation, ctx::Json attributes, ctx::Json options, const char* owner)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	if (!initialized) {
		__trigger_item_list.push_back(trigger_item_format_s(subject, operation, attributes, options, (owner)? owner : ""));
	} else {
		ctx::trigger::TemplateManager* tmplMgr = ctx::trigger::TemplateManager::getInstance();
		IF_FAIL_RETURN_TAG(tmplMgr, false, _E, "Memory allocation failed");
		tmplMgr->registerTemplate(subject, operation, attributes, options, owner);
	}

	return true;
}

bool ctx::context_manager_impl::unregister_trigger_item(const char *subject)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	if (!initialized) {
		__trigger_item_list.push_back(trigger_item_format_s(subject));
	} else {
		ctx::trigger::TemplateManager* tmplMgr = ctx::trigger::TemplateManager::getInstance();
		IF_FAIL_RETURN_TAG(tmplMgr, false, _E, "Memory allocation failed");
		tmplMgr->unregisterTemplate(subject);
	}

	return true;
}

bool ctx::context_manager_impl::pop_trigger_item(std::string &subject, int &operation, ctx::Json &attributes, ctx::Json &options, std::string& owner, bool& unregister)
{
	IF_FAIL_RETURN(!__trigger_item_list.empty(), false);

	trigger_item_format_s format = __trigger_item_list.front();
	__trigger_item_list.pop_front();

	subject = format.subject;
	operation = format.operation;
	attributes = format.attributes;
	options = format.options;
	owner = format.owner;
	unregister = format.unregister;

	return true;
}

void ctx::context_manager_impl::assign_request(ctx::RequestInfo* request)
{
	if (handle_custom_request(request)) {
		delete request;
		return;
	}

	auto it = provider_handle_map.find(request->getSubject());
	if (it == provider_handle_map.end()) {
		_W("Unsupported subject");
		request->reply(ERR_NOT_SUPPORTED);
		delete request;
		return;
	}

	if (!it->second->is_allowed(request->getCredentials())) {
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

bool ctx::context_manager_impl::is_supported(const char *subject)
{
	auto it = provider_handle_map.find(subject);
	return (it != provider_handle_map.end());
}

bool ctx::context_manager_impl::is_allowed(const ctx::Credentials *creds, const char *subject)
{
	IF_FAIL_RETURN(creds, true);	/* In case internal requests */
	auto it = provider_handle_map.find(subject);
	IF_FAIL_RETURN(it != provider_handle_map.end(), false);
	return it->second->is_allowed(creds);
}

void ctx::context_manager_impl::_publish(const char* subject, ctx::Json &option, int error, ctx::Json &data_updated)
{
	_I("Publishing '%s'", subject);
	_J("Option", option);

	auto it = provider_handle_map.find(subject);
	IF_FAIL_VOID(it != provider_handle_map.end());

	it->second->publish(option, error, data_updated);
}

void ctx::context_manager_impl::_reply_to_read(const char* subject, ctx::Json &option, int error, ctx::Json &data_read)
{
	_I("Sending data of '%s'", subject);
	_J("Option", option);
	_J("Data", data_read);

	auto it = provider_handle_map.find(subject);
	IF_FAIL_VOID(it != provider_handle_map.end());

	it->second->reply_to_read(option, error, data_read);
}

struct published_data_s {
	int type;
	ctx::context_manager_impl *mgr;
	std::string subject;
	int error;
	ctx::Json option;
	ctx::Json data;
	published_data_s(int t, ctx::context_manager_impl *m, const char* s, ctx::Json& o, int e, ctx::Json& d)
		: type(t), mgr(m), subject(s), error(e)
	{
		option = o.str();
		data = d.str();
	}
};

gboolean ctx::context_manager_impl::thread_switcher(gpointer data)
{
	published_data_s *tuple = static_cast<published_data_s*>(data);

	switch (tuple->type) {
	case REQ_SUBSCRIBE:
		tuple->mgr->_publish(tuple->subject.c_str(), tuple->option, tuple->error, tuple->data);
		break;
	case REQ_READ:
		tuple->mgr->_reply_to_read(tuple->subject.c_str(), tuple->option, tuple->error, tuple->data);
		break;
	default:
		_W("Invalid type");
	}

	delete tuple;
	return FALSE;
}

bool ctx::context_manager_impl::publish(const char* subject, ctx::Json& option, int error, ctx::Json& data_updated)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	published_data_s *tuple = new(std::nothrow) published_data_s(REQ_SUBSCRIBE, this, subject, option, error, data_updated);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(thread_switcher, tuple);

	return true;
}

bool ctx::context_manager_impl::reply_to_read(const char* subject, ctx::Json& option, int error, ctx::Json& data_read)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	published_data_s *tuple = new(std::nothrow) published_data_s(REQ_READ, this, subject, option, error, data_read);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(thread_switcher, tuple);

	return true;
}

bool ctx::context_manager_impl::handle_custom_request(ctx::RequestInfo* request)
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

		error = ctx::custom_context_provider::add_item(subj, name, tmpl, pkg_id);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_REMOVE) {
		error = ctx::custom_context_provider::remove_item(subj);
		if (error == ERR_NONE) {
			ctx::Json data;
			data.set(NULL, CT_CUSTOM_SUBJECT, subj);
			request->reply(error, data);
		}
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_CUSTOM_PUBLISH) {
		ctx::Json fact;
		desc.get(NULL, CT_CUSTOM_FACT, &fact);

		error = ctx::custom_context_provider::publish_data(subj, fact);
	}

	request->reply(error);
	return true;
}
