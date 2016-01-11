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
#include "timer.h"
#include "timer_types.h"
#include "context_listener_iface.h"

static int last_rid;
static int last_err;
static ctx::json last_data_read;

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
	delete timer;
}

bool ctx::context_monitor::init(ctx::context_manager_impl* ctx_mgr)
{
	_context_mgr = ctx_mgr;

/*	timer = new(std::nothrow) trigger_timer(_trigger);	TODO
	IF_FAIL_RETURN_TAG(timer, false, _E, "Memory allocation failed");*/

	return true;
}

int ctx::context_monitor::subscribe(int rule_id, std::string subject, ctx::json option, context_listener_iface* listener)
{
	if (subject.compare(TIMER_EVENT_SUBJECT) == 0) {
		// option is event json in case of ON_TIME
//		return timer->subscribe(event);
		return ERR_NONE;
	}

	int req_id = _subscribe(subject.c_str(), &option, listener, true);
	IF_FAIL_RETURN_TAG(req_id > 0, ERR_OPERATION_FAILED, _E, "Subscribe event failed");
	_D(YELLOW("Subscribe event(rule%d). req%d"), rule_id, req_id);

	return ERR_NONE;
}

int ctx::context_monitor::_subscribe(const char* subject, json* option, context_listener_iface* listener, bool wait_response)
{
	IF_FAIL_RETURN(subject, ERR_INVALID_PARAMETER);

	int rid = find_sub(subject, option);
	if (rid > 0) {
		add_listener(rid, listener);
		_D("Duplicated request for %s", subject);
		return rid;
	}

	rid = generate_req_id();

	fact_request *req = new(std::nothrow) fact_request(REQ_SUBSCRIBE,
			rid, subject, option ? option->str().c_str() : NULL, wait_response ? this : NULL);
	IF_FAIL_RETURN_TAG(req, -1, _E, "Memory allocation failed");

	send_request(req);	//TODO: what happens if the request actually takes more time than the timeout
	add_sub(rid, subject, option, listener);

	IF_FAIL_RETURN_TAG(wait_response, rid, _D, "Ignoring response for %s", subject);

	if (last_err != ERR_NONE) {
		remove_sub(rid);
		_E("Subscription request failed: %#x", last_err);
		return -1;
	}

	return rid;
}

int ctx::context_monitor::unsubscribe(int rule_id, std::string subject, ctx::json option, context_listener_iface* listener)
{
	if (subject.compare(TIMER_EVENT_SUBJECT) == 0) {
//		return timer->unsubscribe(option);
		return ERR_NONE;
	}

	int rid = find_sub(subject.c_str(), &option);
	if (rid < 0) {
		_D("Invalid unsubscribe request");
		return ERR_INVALID_PARAMETER;
	}

	if (remove_listener(rid, listener) <= 0) {
		_unsubscribe(subject.c_str(), rid);
	}
	_D(YELLOW("Unsubscribe event(rule%d). req%d"), rule_id, rid);

	return ERR_NONE;
}

void ctx::context_monitor::_unsubscribe(const char *subject, int subscription_id)
{
	fact_request *req = new(std::nothrow) fact_request(REQ_UNSUBSCRIBE, subscription_id, subject, NULL, NULL);
	IF_FAIL_VOID_TAG(req, _E, "Memory allocation failed");

	send_request(req);
	remove_sub(subscription_id);
}

bool ctx::context_monitor::send_request(fact_request* req)
{
	_context_mgr->assign_request(req);
	return false;
}

int ctx::context_monitor::read(std::string subject, json option, ctx::json* result)
{
	if (subject.compare(TIMER_CONDITION_SUBJECT) == 0) {
//		return timer->read(result);	TODO
		return ERR_NONE;
	}

/*	context_fact fact;
	bool ret = _read(subject.c_str(), &option, fact);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Read fact failed");

	*result = fact.get_data();
*/
	return ERR_NONE;
}

/*bool ctx::context_monitor::_read(const char* subject, json* option, context_fact& fact)
{
	// TODO implement read async
	IF_FAIL_RETURN(subject, false);

	int rid = generate_req_id();

	fact_request *req = new(std::nothrow) fact_request(REQ_READ_SYNC,
			rid, subject, option ? option->str().c_str() : NULL, this);
	IF_FAIL_RETURN_TAG(req, false, _E, "Memory allocation failed");

	send_request(req);

	if (last_err != ERR_NONE) {
		_E("Read request failed: %#x", last_err);
		return false;
	}

	fact.set_req_id(rid);
	fact.set_subject(subject);
	fact.set_data(last_data_read);
	last_data_read = EMPTY_JSON_OBJECT;

	return true;
}
*/
bool ctx::context_monitor::is_supported(std::string subject)
{
	if (subject.compare(TIMER_EVENT_SUBJECT) == 0
			|| subject.compare(TIMER_CONDITION_SUBJECT) == 0) {
		return true;
	}

	return _context_mgr->is_supported(subject.c_str());
}

bool ctx::context_monitor::is_allowed(const char *client, const char *subject)
{
	if (STR_EQ(subject, TIMER_EVENT_SUBJECT))
		return true;
		//TODO: re-implement above in the proper 3.0 style
		//		return privilege_manager::is_allowed(client, PRIV_ALARM_SET);

	if (STR_EQ(subject, TIMER_CONDITION_SUBJECT))
		return true;

	//TODO: re-implement this in the proper 3.0 style
	//return _context_mgr->is_allowed(client, subject);
	return true;
}

bool ctx::context_monitor::get_fact_definition(std::string &subject, int &operation, ctx::json &attributes, ctx::json &options)
{
	return _context_mgr->pop_trigger_item(subject, operation, attributes, options);
}

int ctx::context_monitor::find_sub(const char* subject, json* option)
{
	// @return	request id
	json opt_j;
	if (option) {
		opt_j = *option;
	}

	for (subscr_map_t::iterator it = subscr_map.begin(); it != subscr_map.end(); ++it) {
		if ((*(it->second)).subject == subject && (*(it->second)).option == opt_j) {
			return it->first;
		}
	}

	return -1;
}

bool ctx::context_monitor::add_sub(int sid, const char* subject, json* option, context_listener_iface* listener)
{
	subscr_info_s *info = new(std::nothrow) subscr_info_s(sid, subject, option);
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");
	info->listener_list.push_back(listener);

	subscr_map[sid] = info;
	return true;
}

void ctx::context_monitor::remove_sub(const char* subject, json* option)
{
	json opt_j;
	if (option) {
		opt_j = *option;
	}

	for (subscr_map_t::iterator it = subscr_map.begin(); it != subscr_map.end(); ++it) {
		if ((*(it->second)).subject == subject && (*(it->second)).option == opt_j) {
			delete it->second;
			subscr_map.erase(it);
			return;
		}
	}
}

void ctx::context_monitor::remove_sub(int sid)
{
	subscr_info_s* info = subscr_map[sid];
	info->listener_list.clear();

	delete info;
	subscr_map.erase(sid);

	return;
}

int ctx::context_monitor::add_listener(int sid, context_listener_iface* listener)
{
	// @return	number of listeners for the corresponding sid
	subscr_map_t::iterator it = subscr_map.find(sid);

	subscr_info_s* info = it->second;
	info->listener_list.push_back(listener);

	return info->listener_list.size();
}

int ctx::context_monitor::remove_listener(int sid, context_listener_iface* listener)
{
	// @return	number of listeners for the corresponding sid
	subscr_map_t::iterator it = subscr_map.find(sid);

	subscr_info_s* info = it->second;

	for (listener_list_t::iterator it2 = info->listener_list.begin(); it2 != info->listener_list.end(); ++it2) {
		if (*it2 == listener) {
			info->listener_list.erase(it2);
			break;
		}
	}

	return info->listener_list.size();
}

void ctx::context_monitor::reply_result(int req_id, int error, json* request_result, json* fact)
{
	_D("Request result received: %d", req_id);
	last_rid = req_id;
	last_err = error;
	last_data_read = (fact ? *fact : EMPTY_JSON_OBJECT);
}

void ctx::context_monitor::publish_fact(int req_id, int error, const char* subject, json* option, json* fact)
{
	_D(YELLOW("Fact received: subject(%s), option(%s), fact(%s)"), subject, option->str().c_str(), fact->str().c_str());

	subscr_map_t::iterator it = subscr_map.find(req_id);
	IF_FAIL_VOID_TAG(it != subscr_map.end(), _E, "Request id not found");

	subscr_info_s* info = it->second;
	for (listener_list_t::iterator it2 = info->listener_list.begin(); it2 != info->listener_list.end(); ++it2) {
		(*it2)->on_event_received(subject, *option, *fact);
	}

}
