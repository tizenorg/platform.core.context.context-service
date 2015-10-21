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
#include <json.h>
#include <provider_iface.h>
#include "server.h"
#include "access_control/privilege.h"
#include "request.h"
#include "provider.h"
#include "context_mgr_impl.h"

/* Context Providers */
#include <internal/device_context_provider.h>
#include <internal/statistics_context_provider.h>
#include <internal/place_context_provider.h>

struct trigger_item_format_s {
	std::string subject;
	int operation;
	ctx::json attributes;
	ctx::json options;
	trigger_item_format_s(std::string subj, int ops, ctx::json attr, ctx::json opt)
		: subject(subj), operation(ops), attributes(attr), options(opt) {}
};

static std::list<trigger_item_format_s> __trigger_item_list;

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

bool ctx::context_manager_impl::register_trigger_item(const char *subject, int operation, ctx::json attributes, ctx::json options)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");
	__trigger_item_list.push_back(trigger_item_format_s(subject, operation, attributes, options));
	return true;
}

bool ctx::context_manager_impl::pop_trigger_item(std::string &subject, int &operation, ctx::json &attributes, ctx::json &options)
{
	IF_FAIL_RETURN(!__trigger_item_list.empty(), false);

	trigger_item_format_s format = __trigger_item_list.front();
	__trigger_item_list.pop_front();

	subject = format.subject;
	operation = format.operation;
	attributes = format.attributes;
	options = format.options;

	return true;
}

void ctx::context_manager_impl::assign_request(ctx::request_info* request)
{
	auto it = provider_handle_map.find(request->get_subject());
	if (it == provider_handle_map.end()) {
		_W("Unsupported subject");
		request->reply(ERR_NOT_SUPPORTED);
		delete request;
		return;
	}

	if (!it->second->is_allowed(request->get_credentials())) {
		_W("Permission denied");
		request->reply(ERR_PERMISSION_DENIED);
		delete request;
		return;
	}

	switch (request->get_type()) {
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

bool ctx::context_manager_impl::is_allowed(const ctx::credentials *creds, const char *subject)
{
	IF_FAIL_RETURN(creds, true);	/* In case internal requests */
	auto it = provider_handle_map.find(subject);
	IF_FAIL_RETURN(it != provider_handle_map.end(), false);
	return it->second->is_allowed(creds);
}

void ctx::context_manager_impl::_publish(const char* subject, ctx::json &option, int error, ctx::json &data_updated)
{
	_I("Publishing '%s'", subject);
	_J("Option", option);

	auto it = provider_handle_map.find(subject);
	IF_FAIL_VOID(it != provider_handle_map.end());

	it->second->publish(option, error, data_updated);
}

void ctx::context_manager_impl::_reply_to_read(const char* subject, ctx::json &option, int error, ctx::json &data_read)
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
	ctx::json option;
	ctx::json data;
	published_data_s(int t, ctx::context_manager_impl *m, const char* s, ctx::json& o, int e, ctx::json& d)
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

bool ctx::context_manager_impl::publish(const char* subject, ctx::json& option, int error, ctx::json& data_updated)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	published_data_s *tuple = new(std::nothrow) published_data_s(REQ_SUBSCRIBE, this, subject, option, error, data_updated);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(thread_switcher, tuple);

	return true;
}

bool ctx::context_manager_impl::reply_to_read(const char* subject, ctx::json& option, int error, ctx::json& data_read)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	published_data_s *tuple = new(std::nothrow) published_data_s(REQ_READ, this, subject, option, error, data_read);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(thread_switcher, tuple);

	return true;
}
