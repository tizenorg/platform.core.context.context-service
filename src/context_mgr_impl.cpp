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
#include "zone_util_impl.h"
#include "access_control/privilege.h"

/* Analyzer Headers */
#include <internal/device_context_provider.h>
#include <internal/app_statistics_provider.h>
#include <internal/social_statistics_provider.h>
#include <internal/media_statistics_provider.h>
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
	IF_FAIL_RETURN_TAG(provider_list.size()==0, false, _W, "Re-initialization");

	try {
		/* List of all providers */
		load_provider(new ctx::device_context_provider());
		load_provider(new ctx::app_statistics_provider());
		load_provider(new ctx::social_statistics_provider());
		load_provider(new ctx::media_statistics_provider());
		load_provider(new ctx::place_context_provider());

	} catch (std::bad_alloc& ba) {
		_E("Analyzer loading failed (bad alloc)");
		return false;
	} catch (int e) {
		_E("Analyzer loading failed (%#x)", e);
		return false;
	}

	return true;
}

void ctx::context_manager_impl::release()
{
	subject_provider_map.clear();

	for (provider_list_t::iterator it = provider_list.begin(); it != provider_list.end(); ++it) {
		delete *it;
	}
	provider_list.clear();

	for (request_list_t::iterator it = subscribe_request_list.begin(); it != subscribe_request_list.end(); ++it) {
		delete *it;
	}
	subscribe_request_list.clear();

	for (request_list_t::iterator it = read_request_list.begin(); it != read_request_list.end(); ++it) {
		delete *it;
	}
	read_request_list.clear();
}

void ctx::context_manager_impl::load_provider(context_provider_iface* provider)
{
	if (!provider) {
		_E("Analyzer NULL");
		throw static_cast<int>(ERR_INVALID_PARAMETER);
	}

	provider_list.push_back(provider);

	if (!provider->init()) {
		_E("Analyzer initialization failed");
		throw ERR_OPERATION_FAILED;
	}
}

bool ctx::context_manager_impl::register_provider(const char* subject, ctx::context_provider_iface* cp)
{
	IF_FAIL_RETURN_TAG(subject && cp, false, _E, "Invalid parameter");

	if (subject_provider_map.find(subject) != subject_provider_map.end()) {
		_E("The provider for the subject '%s' is already registered.", subject);
		return false;
	}

	_I("Registering provider for '%s'", subject);
	subject_provider_map[subject] = cp;

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

bool ctx::context_manager_impl::is_supported(const char* subject, const char* zone)
{
	subject_provider_map_t::iterator it = subject_provider_map.find(subject);
	IF_FAIL_RETURN(it != subject_provider_map.end(), false);

	return it->second->is_supported(subject, zone);
}

void ctx::context_manager_impl::is_supported(request_info *request)
{
	if (is_supported(request->get_subject(), request->get_zone_name()))
		request->reply(ERR_NONE);
	else
		request->reply(ERR_NOT_SUPPORTED);

	delete request;
}

ctx::context_manager_impl::request_list_t::iterator
ctx::context_manager_impl::find_request(request_list_t& r_list, std::string subject, json& option, const char* zone)
{
	return find_request(r_list.begin(), r_list.end(), subject, option, zone);
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
ctx::context_manager_impl::find_request(request_list_t::iterator begin, request_list_t::iterator end, std::string subject, json& option, const char* zone)
{
	//TODO: Do we need to consider the case that the inparam option is a subset of the request description?
	request_list_t::iterator it;
	for (it = begin; it != end; ++it) {
		if (subject == (*it)->get_subject() && option == (*it)->get_description() && STR_EQ(zone, (*it)->get_zone_name())) {
			break;
		}
	}
	return it;
}

ctx::context_provider_iface* ctx::context_manager_impl::get_provider(ctx::request_info *request)
{
	subject_provider_map_t::iterator it = subject_provider_map.find(request->get_subject());
	if (it == subject_provider_map.end()) {
		_E("Unknown subject '%s'", request->get_subject());
		request->reply(ERR_NOT_SUPPORTED);
		delete request;
		return NULL;
	}
	return it->second;
}

bool ctx::context_manager_impl::check_permission(ctx::request_info* request)
{
	const char* app_id = request->get_app_id();
	_D("Peer AppID: %s", app_id);

	if (app_id == NULL) {
		_E("AppID NULL");
		request->reply(ERR_PERMISSION_DENIED);
		delete request;
		return false;
	}

	IF_FAIL_RETURN_TAG(!STR_EQ(app_id, TRIGGER_CLIENT_NAME), true, _D, "Skipping permission check for Trigger");

	scope_zone_joiner sz(request->get_zone_name());

	bool allowed = ctx::privilege_manager::is_allowed(app_id, request->get_subject());
	if (!allowed) {
		_W("Permission denied");
		request->reply(ERR_PERMISSION_DENIED);
		delete request;
		return false;
	}

	return true;
}

void ctx::context_manager_impl::subscribe(ctx::request_info *request)
{
	_I(CYAN("[%s] '%s' subscribes '%s' (RID-%d)"), request->get_zone_name(), request->get_client(), request->get_subject(), request->get_id());
	IF_FAIL_VOID(check_permission(request));

	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	ctx::json request_result;
	int error = provider->subscribe(request->get_subject(), request->get_description().str(), &request_result, request->get_zone_name());

	_D("Analyzer returned %d", error);

	if (!request->reply(error, request_result) || error != ERR_NONE) {
		delete request;
		return;
	}

	subscribe_request_list.push_back(request);
}

void ctx::context_manager_impl::unsubscribe(ctx::request_info *request)
{
	_I(CYAN("[%s] '%s' unsubscribes RID-%d"), request->get_zone_name(), request->get_client(), request->get_id());

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
	if (find_request(subscribe_request_list, req_found->get_subject(), req_found->get_description(), req_found->get_zone_name()) != subscribe_request_list.end()) {
		// Do not stop detecting the subject
		_D("A same request from '%s' exists", req_found->get_client());
		request->reply(ERR_NONE);
		delete request;
		delete req_found;
		return;
	}

	// Find the proper provider
	subject_provider_map_t::iterator ca = subject_provider_map.find(req_found->get_subject());
	if (ca == subject_provider_map.end()) {
		_E("Invalid subject '%s'", req_found->get_subject());
		delete request;
		delete req_found;
		return;
	}

	// Stop detecting the subject
	int error = ca->second->unsubscribe(req_found->get_subject(), req_found->get_description(), req_found->get_zone_name());
	request->reply(error);
	delete request;
	delete req_found;
}

void ctx::context_manager_impl::read(ctx::request_info *request)
{
	_I(CYAN("[%s] '%s' reads '%s' (RID-%d)"), request->get_zone_name(), request->get_client(), request->get_subject(), request->get_id());
	IF_FAIL_VOID(check_permission(request));

	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	ctx::json request_result;
	int error = provider->read(request->get_subject(), request->get_description().str(), &request_result, request->get_zone_name());

	_D("Analyzer returned %d", error);

	if (!request->reply(error, request_result) || error != ERR_NONE) {
		delete request;
		return;
	}

	read_request_list.push_back(request);
}

void ctx::context_manager_impl::write(ctx::request_info *request)
{
	_I(CYAN("[%s] '%s' writes '%s' (RID-%d)"), request->get_zone_name(), request->get_client(), request->get_subject(), request->get_id());
	IF_FAIL_VOID(check_permission(request));

	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	ctx::json request_result;
	int error = provider->write(request->get_subject(), request->get_description(), &request_result, request->get_zone_name());

	_D("Analyzer returned %d", error);

	request->reply(error, request_result);
	delete request;
}

bool ctx::context_manager_impl::_publish(const char* subject, ctx::json option, int error, ctx::json data_updated, const char* zone)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	_I("Publishing '%s'", subject);
	_J("Option", option);

	request_list_t::iterator end = subscribe_request_list.end();
	request_list_t::iterator target = find_request(subscribe_request_list.begin(), end, subject, option, zone);

	while (target != end) {
		if (!(*target)->publish(error, data_updated)) {
			return false;
		}
		target = find_request(++target, end, subject, option, zone);
	}

	return true;
}

bool ctx::context_manager_impl::_reply_to_read(const char* subject, ctx::json option, int error, ctx::json data_read, const char* zone)
{
	IF_FAIL_RETURN_TAG(subject, false, _E, "Invalid parameter");

	_I("Sending data of '%s'", subject);
	_J("Option", option);
	_J("Data", data_read);
	_SI("Zone: '%s'", zone);

	request_list_t::iterator end = read_request_list.end();
	request_list_t::iterator target = find_request(read_request_list.begin(), end, subject, option, zone);
	request_list_t::iterator prev;

	ctx::json dummy;

	while (target != end) {
		(*target)->reply(error, dummy, data_read);
		prev = target;
		target = find_request(++target, end, subject, option, zone);

		delete *prev;
		read_request_list.erase(prev);
	}

	return true;
}

struct published_data_s {
	int type;
	ctx::context_manager_impl *mgr;
	std::string subject;
	std::string zone;
	int error;
	ctx::json option;
	ctx::json data;
	published_data_s(int t, ctx::context_manager_impl *m, const char* s, ctx::json& o, int e, ctx::json& d, const char* z)
		: type(t), mgr(m), subject(s), error(e)
	{
		option = o.str();
		data = d.str();
		zone = z;
	}
};

gboolean ctx::context_manager_impl::thread_switcher(gpointer data)
{
	published_data_s *tuple = static_cast<published_data_s*>(data);

	switch (tuple->type) {
		case REQ_SUBSCRIBE:
			tuple->mgr->_publish(tuple->subject.c_str(), tuple->option, tuple->error, tuple->data, tuple->zone.c_str());
			break;
		case REQ_READ:
			tuple->mgr->_reply_to_read(tuple->subject.c_str(), tuple->option, tuple->error, tuple->data, tuple->zone.c_str());
			break;
		default:
			_W("Invalid type");
	}

	delete tuple;
	return FALSE;
}

bool ctx::context_manager_impl::publish(const char* subject, ctx::json& option, int error, ctx::json& data_updated, const char* zone)
{
	IF_FAIL_RETURN_TAG(subject && zone, false, _E, "Invalid parameter");

	published_data_s *tuple = new(std::nothrow) published_data_s(REQ_SUBSCRIBE, this, subject, option, error, data_updated, zone);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(thread_switcher, tuple);

	return true;
}

bool ctx::context_manager_impl::reply_to_read(const char* subject, ctx::json& option, int error, ctx::json& data_read, const char* zone)
{
	IF_FAIL_RETURN_TAG(subject && zone, false, _E, "Invalid parameter");

	published_data_s *tuple = new(std::nothrow) published_data_s(REQ_READ, this, subject, option, error, data_read, zone);
	IF_FAIL_RETURN_TAG(tuple, false, _E, "Memory allocation failed");

	g_idle_add(thread_switcher, tuple);

	return true;
}
