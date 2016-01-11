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

#include <types_internal.h>
#include "../access_control/privilege.h"
#include "../context_mgr_impl.h"
#include "context_monitor.h"
#include "fact_request.h"
#include "context_listener_iface.h"

static int last_rid;
static int last_err;

ctx::context_monitor *ctx::context_monitor::_instance = NULL;
ctx::context_manager_impl *ctx::context_monitor::_context_mgr = NULL;

static int generate_req_id()
{
	static int req_id = 0;

	if (++req_id < 0) {
		// Overflow handling
		req_id = 1;
	}

	return req_id;
}

ctx::context_monitor::context_monitor()
{
}

ctx::context_monitor::~context_monitor()
{
}

void ctx::context_monitor::set_context_manager(ctx::context_manager_impl* ctx_mgr)
{
	_context_mgr = ctx_mgr;
}

ctx::context_monitor* ctx::context_monitor::get_instance()
{
	IF_FAIL_RETURN_TAG(_context_mgr, NULL, _E, "Context manager is needed");

	IF_FAIL_RETURN(!_instance, _instance);

	_instance = new(std::nothrow) context_monitor();
	IF_FAIL_RETURN_TAG(_instance, NULL, _E, "Memory alllocation failed");

	return _instance;
}

void ctx::context_monitor::destroy()
{
	if (_instance) {
		delete _instance;
		_instance = NULL;
	}
}

int ctx::context_monitor::subscribe(int rule_id, std::string subject, ctx::json option, context_listener_iface* listener)
{
	int req_id = _subscribe(subject.c_str(), &option, listener);
	IF_FAIL_RETURN_TAG(req_id > 0, ERR_OPERATION_FAILED, _E, "Subscribe event failed");
	_D(YELLOW("Subscribe event(rule%d). req%d"), rule_id, req_id);

	return ERR_NONE;
}

int ctx::context_monitor::_subscribe(const char* subject, json* option, context_listener_iface* listener)
{
	IF_FAIL_RETURN(subject, ERR_INVALID_PARAMETER);

	int rid = find_sub(REQ_SUBSCRIBE, subject, option);
	if (rid > 0) {
		add_listener(REQ_SUBSCRIBE, rid, listener);
		_D("Duplicated request for %s", subject);
		return rid;
	}

	rid = generate_req_id();

	fact_request *req = new(std::nothrow) fact_request(REQ_SUBSCRIBE,
			rid, subject, option ? option->str().c_str() : NULL, this);
	IF_FAIL_RETURN_TAG(req, -1, _E, "Memory allocation failed");

	_context_mgr->assign_request(req);
	add_sub(REQ_SUBSCRIBE, rid, subject, option, listener);

	if (last_err != ERR_NONE) {
		remove_sub(REQ_SUBSCRIBE, rid);
		_E("Subscription request failed: %#x", last_err);
		return -1;
	}

	return rid;
}

int ctx::context_monitor::unsubscribe(int rule_id, std::string subject, ctx::json option, context_listener_iface* listener)
{
	int rid = find_sub(REQ_SUBSCRIBE, subject.c_str(), &option);
	if (rid < 0) {
		_D("Invalid unsubscribe request");
		return ERR_INVALID_PARAMETER;
	}

	if (remove_listener(REQ_SUBSCRIBE, rid, listener) <= 0) {
		_unsubscribe(subject.c_str(), rid);
	}
	_D(YELLOW("Unsubscribe event(rule%d). req%d"), rule_id, rid);

	return ERR_NONE;
}

void ctx::context_monitor::_unsubscribe(const char *subject, int subscription_id)
{
	fact_request *req = new(std::nothrow) fact_request(REQ_UNSUBSCRIBE, subscription_id, subject, NULL, NULL);
	IF_FAIL_VOID_TAG(req, _E, "Memory allocation failed");

	_context_mgr->assign_request(req);
	remove_sub(REQ_SUBSCRIBE, subscription_id);
}

int ctx::context_monitor::read(std::string subject, json option, context_listener_iface* listener)
{
	int req_id = _read(subject.c_str(), &option, listener);
	IF_FAIL_RETURN_TAG(req_id > 0, ERR_OPERATION_FAILED, _E, "Read condition failed");
	_D(YELLOW("Read condition(%s). req%d"), subject.c_str(), req_id);

	return ERR_NONE;
}

int ctx::context_monitor::_read(const char* subject, json* option, context_listener_iface* listener)
{
	IF_FAIL_RETURN(subject, ERR_INVALID_PARAMETER);

	int rid = find_sub(REQ_READ, subject, option);
	if (rid > 0) {
		add_listener(REQ_READ, rid, listener);
		_D("Duplicated request for %s", subject);
		return rid;
	}

	rid = generate_req_id();

	fact_request *req = new(std::nothrow) fact_request(REQ_READ,
			rid, subject, option ? option->str().c_str() : NULL, this);
	IF_FAIL_RETURN_TAG(req, -1, _E, "Memory allocation failed");

	_context_mgr->assign_request(req);
	add_sub(REQ_READ, rid, subject, option, listener);

	if (last_err != ERR_NONE) {
		_E("Read request failed: %#x", last_err);
		return -1;
	}

	return rid;
}

bool ctx::context_monitor::is_supported(std::string subject)
{
	return _context_mgr->is_supported(subject.c_str());
}

bool ctx::context_monitor::is_allowed(const char *client, const char *subject)
{
	//TODO: re-implement this in the proper 3.0 style
	//return _context_mgr->is_allowed(client, subject);
	return true;
}

int ctx::context_monitor::find_sub(request_type type, const char* subject, json* option)
{
	// @return	request id
	subscr_map_t* map = (type == REQ_SUBSCRIBE)? &subscr_map : &read_map;

	json opt_j;
	if (option) {
		opt_j = *option;
	}

	for (subscr_map_t::iterator it = map->begin(); it != map->end(); ++it) {
		if ((*(it->second)).subject == subject && (*(it->second)).option == opt_j) {
			return it->first;
		}
	}

	return -1;
}

bool ctx::context_monitor::add_sub(request_type type, int sid, const char* subject, json* option, context_listener_iface* listener)
{
	subscr_map_t* map = (type == REQ_SUBSCRIBE)? &subscr_map : &read_map;

	subscr_info_s *info = new(std::nothrow) subscr_info_s(sid, subject, option);
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");
	info->listener_list.push_back(listener);

	map->insert(std::pair<int, subscr_info_s*>(sid, info));
	return true;
}

void ctx::context_monitor::remove_sub(request_type type, const char* subject, json* option)
{
	subscr_map_t* map = (type == REQ_SUBSCRIBE)? &subscr_map : &read_map;

	json opt_j;
	if (option) {
		opt_j = *option;
	}

	for (subscr_map_t::iterator it = map->begin(); it != map->end(); ++it) {
		if ((*(it->second)).subject == subject && (*(it->second)).option == opt_j) {
			delete it->second;
			map->erase(it);
			return;
		}
	}
}

void ctx::context_monitor::remove_sub(request_type type, int sid)
{
	subscr_map_t* map = (type == REQ_SUBSCRIBE)? &subscr_map : &read_map;

	subscr_info_s* info = map->at(sid);
	info->listener_list.clear();

	delete info;
	map->erase(sid);

	return;
}

int ctx::context_monitor::add_listener(request_type type, int sid, context_listener_iface* listener)
{
	// @return	number of listeners for the corresponding sid
	subscr_map_t* map = (type == REQ_SUBSCRIBE)? &subscr_map : &read_map;

	subscr_map_t::iterator it = map->find(sid);

	subscr_info_s* info = it->second;
	info->listener_list.push_back(listener);

	return info->listener_list.size();
}

int ctx::context_monitor::remove_listener(request_type type, int sid, context_listener_iface* listener)
{
	// @return	number of listeners for the corresponding sid
	subscr_map_t* map = (type == REQ_SUBSCRIBE)? &subscr_map : &read_map;

	subscr_map_t::iterator it = map->find(sid);

	subscr_info_s* info = it->second;

	for (listener_list_t::iterator it2 = info->listener_list.begin(); it2 != info->listener_list.end(); ++it2) {
		if (*it2 == listener) {
			info->listener_list.erase(it2);
			break;
		}
	}

	return info->listener_list.size();
}

void ctx::context_monitor::reply_result(int req_id, int error, json* request_result)
{
	_D("Request result received: %d", req_id);

	last_rid = req_id;
	last_err = error;
}

void ctx::context_monitor::reply_result(int req_id, int error, const char* subject, json* option, json* fact)
{
	_D(YELLOW("Condition received: subject(%s), option(%s), fact(%s)"), subject, option->str().c_str(), fact->str().c_str());

	subscr_map_t::iterator it = read_map.find(req_id);
	IF_FAIL_VOID_TAG(it != read_map.end(), _E, "Request id not found");

	subscr_info_s* info = it->second;
	for (listener_list_t::iterator it2 = info->listener_list.begin(); it2 != info->listener_list.end(); ++it2) {
		(*it2)->on_condition_received(subject, *option, *fact);
	}

	remove_sub(REQ_READ, req_id);
}

void ctx::context_monitor::publish_fact(int req_id, int error, const char* subject, json* option, json* fact)
{
	_D(YELLOW("Event received: subject(%s), option(%s), fact(%s)"), subject, option->str().c_str(), fact->str().c_str());

	subscr_map_t::iterator it = subscr_map.find(req_id);
	IF_FAIL_VOID_TAG(it != subscr_map.end(), _E, "Request id not found");

	subscr_info_s* info = it->second;
	for (listener_list_t::iterator it2 = info->listener_list.begin(); it2 != info->listener_list.end(); ++it2) {
		(*it2)->on_event_received(subject, *option, *fact);
	}
}
