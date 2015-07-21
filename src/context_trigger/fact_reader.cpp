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

#include <scope_mutex.h>
#include "trigger.h"
#include "fact_request.h"
#include "fact_reader.h"

#define CLIENT_NAME			TRIGGER_CLIENT_NAME
#define COND_END_TIME(T)	(g_get_monotonic_time() + (T) * G_TIME_SPAN_SECOND)
#define SUBSCRIBE_TIMEOUT	3
#define READ_TIMEOUT		10

static GMutex request_mutex;
static GCond request_cond;
static int last_rid;
static int last_err;
static ctx::json last_data_read;

ctx::context_manager_impl *ctx::fact_reader::_context_mgr = NULL;
ctx::context_trigger *ctx::fact_reader::_trigger = NULL;

static int generate_req_id()
{
	static int req_id = 0;

	if (++req_id < 0) {
		// Overflow handling
		req_id = 1;
	}

	return req_id;
}

ctx::fact_reader::fact_reader(context_manager_impl* mgr, context_trigger* trigger)
{
	_context_mgr = mgr;
	_trigger = trigger;
}

ctx::fact_reader::~fact_reader()
{
	for (subscr_list_t::iterator it = subscr_list.begin(); it != subscr_list.end(); ++it) {
		delete *it;
	}
	subscr_list.clear();
}

int ctx::fact_reader::find_sub(const char* subject, json* option)
{
	json opt_j;
	if (option) {
		opt_j = *option;
	}

	for (subscr_list_t::iterator it = subscr_list.begin(); it != subscr_list.end(); ++it) {
		if ((*it)->subject == subject && (*it)->option == opt_j) {
			return (*it)->sid;
		}
	}

	return -1;
}

bool ctx::fact_reader::add_sub(int sid, const char* subject, json* option)
{
	subscr_info_s *info = new(std::nothrow) subscr_info_s(sid, subject, option);
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");

	subscr_list.push_back(info);
	return true;
}

void ctx::fact_reader::remove_sub(const char* subject, json* option)
{
	json opt_j;
	if (option) {
		opt_j = *option;
	}

	for (subscr_list_t::iterator it = subscr_list.begin(); it != subscr_list.end(); ++it) {
		if ((*it)->subject == subject && (*it)->option == opt_j) {
			delete *it;
			subscr_list.erase(it);
			return;
		}
	}
}

void ctx::fact_reader::remove_sub(int sid)
{
	for (subscr_list_t::iterator it = subscr_list.begin(); it != subscr_list.end(); ++it) {
		if ((*it)->sid == sid) {
			delete *it;
			subscr_list.erase(it);
			return;
		}
	}
}

gboolean ctx::fact_reader::send_request(gpointer data)
{
	fact_request *req = static_cast<fact_request*>(data);
	_context_mgr->assign_request(req);
	return FALSE;
}

bool ctx::fact_reader::is_supported(const char* subject)
{
	return _context_mgr->is_supported(subject);
}

int ctx::fact_reader::subscribe(const char* subject, json* option, bool wait_response)
{
	IF_FAIL_RETURN(subject, ERR_INVALID_PARAMETER);

	ctx::scope_mutex sm(&request_mutex);

	int rid = find_sub(subject, option);
	if (rid > 0) {
		_D("Duplicated request for %s", subject);
		return rid;
	}

	rid = generate_req_id();

	fact_request *req = new(std::nothrow) fact_request(REQ_SUBSCRIBE, CLIENT_NAME,
			rid, subject, option ? option->str().c_str() : NULL, wait_response ? this : NULL);
	IF_FAIL_RETURN_TAG(req, -1, _E, "Memory allocation failed");

	g_idle_add(send_request, req);
	add_sub(rid, subject, option);

	IF_FAIL_RETURN_TAG(wait_response, rid, _D, "Ignoring response for %s", subject);

	while (last_rid != rid) {
		if (!g_cond_wait_until(&request_cond, &request_mutex, COND_END_TIME(SUBSCRIBE_TIMEOUT))) {
			_E("Timeout: subject %s", subject);
			//TODO: what happens if the request actually takes more time than the timeout
			remove_sub(rid);
			return -1;
		}
	}

	if (last_err != ERR_NONE) {
		remove_sub(rid);
		_E("Subscription request failed: %#x", last_err);
		return -1;
	}

	return rid;
}

void ctx::fact_reader::unsubscribe(const char* subject, json* option)
{
	IF_FAIL_VOID(subject);

	ctx::scope_mutex sm(&request_mutex);

	int rid = find_sub(subject, option);
	IF_FAIL_VOID_TAG(rid > 0, _W, "Unknown subscription for %s", subject);

	unsubscribe(rid);
}

void ctx::fact_reader::unsubscribe(int subscription_id)
{
	fact_request *req = new(std::nothrow) fact_request(REQ_UNSUBSCRIBE, CLIENT_NAME, subscription_id, "", NULL, NULL);
	IF_FAIL_VOID_TAG(req, _E, "Memory allocation failed");

	g_idle_add(send_request, req);
	remove_sub(subscription_id);
}

bool ctx::fact_reader::read(const char* subject, json* option, context_fact& fact)
{
	IF_FAIL_RETURN(subject, false);

	ctx::scope_mutex sm(&request_mutex);

	int rid = generate_req_id();

	fact_request *req = new(std::nothrow) fact_request(REQ_READ_SYNC, CLIENT_NAME,
			rid, subject, option ? option->str().c_str() : NULL, this);
	IF_FAIL_RETURN_TAG(req, false, _E, "Memory allocation failed");

	g_idle_add(send_request, req);

	while (last_rid != rid) {
		if (!g_cond_wait_until(&request_cond, &request_mutex, COND_END_TIME(READ_TIMEOUT))) {
			_E("Timeout: subject %s", subject);
			//TODO: what happens if the request actually takes more time than the timeout
			return false;
		}
	}

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

void ctx::fact_reader::reply_result(int req_id, int error, json* request_result, json* fact)
{
	ctx::scope_mutex sm(&request_mutex);

	last_rid = req_id;
	last_err = error;
	last_data_read = (fact ? *fact : EMPTY_JSON_OBJECT);

	g_cond_signal(&request_cond);
}

void ctx::fact_reader::publish_fact(int req_id, int error, const char* subject, json* option, json* fact)
{
	_trigger->push_fact(req_id, error, subject, *option, *fact);
}
