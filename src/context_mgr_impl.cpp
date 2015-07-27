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
#include <string>

#include <types_internal.h>
#include <json.h>
#include <provider_iface.h>
#include "server.h"
#include "context_mgr_impl.h"
#include "access_control/privilege.h"

/* Context Providers */
#include <internal/device_context_provider.h>
#include <internal/statistics_context_provider.h>
#include <internal/place_context_provider.h>

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
	for (provider_map_t::iterator it = provider_map.begin(); it != provider_map.end(); ++it) {
		it->second.destroy(it->second.data);
	}
	provider_map.clear();

	for (request_list_t::iterator it = subscribe_request_list.begin(); it != subscribe_request_list.end(); ++it) {
		delete *it;
	}
	subscribe_request_list.clear();

	for (request_list_t::iterator it = read_request_list.begin(); it != read_request_list.end(); ++it) {
		delete *it;
	}
	read_request_list.clear();
}

bool ctx::context_manager_impl::register_provider(const char *subject, ctx::context_provider_info &provider_info)
{
	if (provider_map.find(subject) != provider_map.end()) {
		_E("The provider for the subject '%s' is already registered.", subject);
		return false;
	}

	_SI("Subj: %s, Priv: %s", subject, provider_info.privilege);
	provider_map[subject] = provider_info;

	return true;
}

void ctx::context_manager_impl::assign_request(ctx::request_info* request)
{
	switch (request->get_type()) {
	case REQ_SUBSCRIBE:
		subscribe(request);
		break;
	case REQ_UNSUBSCRIBE:
		unsubscribe(request);
		break;
	case REQ_READ:
	case REQ_READ_SYNC:
		read(request);
		break;
	case REQ_WRITE:
		write(request);
		break;
	case REQ_SUPPORT:
		is_supported(request);
		break;
	default:
		_E("Invalid type of request");
		delete request;
	}
}

bool ctx::context_manager_impl::is_supported(const char *subject)
{
	provider_map_t::iterator it = provider_map.find(subject);
	return (it != provider_map.end());
}

void ctx::context_manager_impl::is_supported(request_info *request)
{
	if (is_supported(request->get_subject()))
		request->reply(ERR_NONE);
	else
		request->reply(ERR_NOT_SUPPORTED);

	delete request;
}

bool ctx::context_manager_impl::is_allowed(const char *client, const char *subject)
{
	provider_map_t::iterator it = provider_map.find(subject);
	IF_FAIL_RETURN(it != provider_map.end(), false);
	IF_FAIL_RETURN(ctx::privilege_manager::is_allowed(client, it->second.privilege), false);
	return true;
}

ctx::context_manager_impl::request_list_t::iterator
ctx::context_manager_impl::find_request(request_list_t& r_list, std::string subject, json& option)
{
	return find_request(r_list.begin(), r_list.end(), subject, option);
}

ctx::context_manager_impl::request_list_t::iterator
ctx::context_manager_impl::find_request(request_list_t& r_list, std::string client, int req_id)
{
	request_list_t::iterator it;
	for (it = r_list.begin(); it != r_list.end(); ++it) {
		if (client == (*it)->get_client() && req_id == (*it)->get_id()) {
			break;
		}
	}
	return it;
}

ctx::context_manager_impl::request_list_t::iterator
ctx::context_manager_impl::find_request(request_list_t::iterator begin, request_list_t::iterator end, std::string subject, json& option)
{
	//TODO: Do we need to consider the case that the inparam option is a subset of the request description?
	request_list_t::iterator it;
	for (it = begin; it != end; ++it) {
		if (subject == (*it)->get_subject() && option == (*it)->get_description()) {
			break;
		}
	}
	return it;
}

ctx::context_provider_iface *ctx::context_manager_impl::get_provider(ctx::request_info *request)
{
	provider_map_t::iterator it = provider_map.find(request->get_subject());

	if (it == provider_map.end()) {
		_W("Unsupported subject");
		request->reply(ERR_NOT_SUPPORTED);
		delete request;
		return NULL;
	}

	if (!ctx::privilege_manager::is_allowed(request->get_client(), it->second.privilege)) {
		_W("Permission denied");
		request->reply(ERR_PERMISSION_DENIED);
		delete request;
		return NULL;
	}

	return it->second.create(it->second.data);
}

void ctx::context_manager_impl::subscribe(ctx::request_info *request)
{
	_I(CYAN("'%s' subscribes '%s' (RID-%d)"), request->get_client(), request->get_subject(), request->get_id());

	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	ctx::json request_result;
	int error = provider->subscribe(request->get_subject(), request->get_description().str(), &request_result);

	_D("Analyzer returned %d", error);

	if (!request->reply(error, request_result) || error != ERR_NONE) {
		delete request;
		return;
	}

	subscribe_request_list.push_back(request);
}

void ctx::context_manager_impl::unsubscribe(ctx::request_info *request)
{
	_I(CYAN("'%s' unsubscribes RID-%d"), request->get_client(), request->get_id());

	// Search the subscribe request to be removed
	request_list_t::iterator target = find_request(subscribe_request_list, request->get_client(), request->get_id());
	if (target == subscribe_request_list.end()) {
		_W("Unknown request");
		delete request;
		return;
	}

	// Keep the pointer to the request found
	request_info *req_found = *target;

	// Remove the request from the list
	subscribe_request_list.erase(target);

	// Check if there exist the same requests
	if (find_request(subscribe_request_list, req_found->get_subject(), req_found->get_description()) != subscribe_request_list.end()) {
		// Do not stop detecting the subject
		_D("A same request from '%s' exists", req_found->get_client());
		request->reply(ERR_NONE);
		delete request;
		delete req_found;
		return;
	}

	// Find the proper provider
	provider_map_t::iterator ca = provider_map.find(req_found->get_subject());
	if (ca == provider_map.end()) {
		_E("Invalid subject '%s'", req_found->get_subject());
		delete request;
		delete req_found;
		return;
	}

	// Stop detecting the subject
	int error = ca->second.create(ca->second.data)->unsubscribe(req_found->get_subject(), req_found->get_description());
	request->reply(error);
	delete request;
	delete req_found;
}

void ctx::context_manager_impl::read(ctx::request_info *request)
{
	_I(CYAN("'%s' reads '%s' (RID-%d)"), request->get_client(), request->get_subject(), request->get_id());

	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	ctx::json request_result;
	int error = provider->read(request->get_subject(), request->get_description().str(), &request_result);

	_D("Analyzer returned %d", error);

	if (!request->reply(error, request_result) || error != ERR_NONE) {
		delete request;
		return;
	}

	read_request_list.push_back(request);
}

void ctx::context_manager_impl::write(ctx::request_info *request)
{
	_I(CYAN("'%s' writes '%s' (RID-%d)"), request->get_client(), request->get_subject(), request->get_id());

	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	ctx::json request_result;
	int error = provider->write(request->get_subject(), request->get_description(), &request_result);

	_D("Analyzer returned %d", error);

	request->reply(error, request_result);
	delete request;
}

bool ctx::context_manager_impl::_publish(const char* subject, ctx::json option, int error, ctx::json data_updated)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	_I("Publishing '%s'", subject);
	_J("Option", option);

	request_list_t::iterator end = subscribe_request_list.end();
	request_list_t::iterator target = find_request(subscribe_request_list.begin(), end, subject, option);

	while (target != end) {
		if (!(*target)->publish(error, data_updated)) {
			return false;
		}
		target = find_request(++target, end, subject, option);
	}

	return true;
}

bool ctx::context_manager_impl::_reply_to_read(const char* subject, ctx::json option, int error, ctx::json data_read)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	_I("Sending data of '%s'", subject);
	_J("Option", option);
	_J("Data", data_read);

	request_list_t::iterator end = read_request_list.end();
	request_list_t::iterator target = find_request(read_request_list.begin(), end, subject, option);
	request_list_t::iterator prev;

	ctx::json dummy;

	while (target != end) {
		(*target)->reply(error, dummy, data_read);
		prev = target;
		target = find_request(++target, end, subject, option);

		delete *prev;
		read_request_list.erase(prev);
	}

	return true;
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
