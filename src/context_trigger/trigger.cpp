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
#include <context_trigger_types_internal.h>
#include "fact_reader.h"
#include "trigger.h"
#include "rule_manager.h"

ctx::context_trigger::context_trigger()
	: _reader(NULL)
{
}

ctx::context_trigger::~context_trigger()
{
}

bool ctx::context_trigger::init(ctx::context_manager_impl* mgr)
{
	// Do the necessary initialization process.
	// This function is called from the main thread during the service launching process.
	_reader = new(std::nothrow) fact_reader(mgr, this);
	IF_FAIL_RETURN_TAG(_reader, false, _E, "Memory allocation failed");

	_D("Starting Context Trigger Thread");
	IF_FAIL_RETURN(start(), false);

	push_thread_event(ETYPE_INITIALIZE, NULL);

	return true;
}

void ctx::context_trigger::release()
{
	_D("Stopping Context Trigger Thread");
	stop();

	// Release the occupied resources.
	// This function is called from the main thread during the service termination process.
	delete _reader;
	_reader = NULL;
}

void ctx::context_trigger::on_thread_event_popped(int type, void* data)
{
	switch (type) {
		case ETYPE_REQUEST:
			IF_FAIL_VOID(data);
			process_request(static_cast<request_info*>(data));
			break;
		case ETYPE_FACT:
			IF_FAIL_VOID(data);
			process_fact(static_cast<context_fact*>(data));
			break;
		case ETYPE_INITIALIZE:
			process_initialize();
			break;
		default:
			_W("Unknown event type");
			return;
	}

	delete_thread_event(type, data);
}

void ctx::context_trigger::delete_thread_event(int type, void* data)
{
	IF_FAIL_VOID(data);

	switch (type) {
		case ETYPE_REQUEST:
			{
				std::string subject = static_cast<ctx::request_info*>(data)->get_subject();
				if (subject != CONTEXT_TRIGGER_SUBJECT_ENABLE) {
					delete (static_cast<request_info*>(data));
				}
			}
			return;
		case ETYPE_FACT:
			delete (static_cast<context_fact*>(data));
			return;
		case ETYPE_INITIALIZE:
			return;
		default:
			_W("Unknown event type");
			return;
	}
}

bool ctx::context_trigger::assign_request(ctx::request_info* request)
{
	std::string subject = request->get_subject();
	if (subject != CONTEXT_TRIGGER_SUBJECT_ADD && subject != CONTEXT_TRIGGER_SUBJECT_REMOVE &&
			subject != CONTEXT_TRIGGER_SUBJECT_ENABLE && subject != CONTEXT_TRIGGER_SUBJECT_DISABLE	&&
			subject != CONTEXT_TRIGGER_SUBJECT_GET && subject != CONTEXT_TRIGGER_SUBJECT_GET_RULE_IDS) {
		return false;
	}

	push_thread_event(ETYPE_REQUEST, request);
	return true;
}

void ctx::context_trigger::process_request(ctx::request_info* request)
{
	// Process the request, and reply to the client if necessary.
	const char* req_sbj = request->get_subject();
	_D("Request is %s", req_sbj);
	std::string subject(req_sbj);

	if (subject == CONTEXT_TRIGGER_SUBJECT_ADD) {
		add_rule(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_REMOVE) {
		remove_rule(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_ENABLE) {
		enable_rule(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_DISABLE) {
		disable_rule(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_GET) {
		get_rule_by_id(request);
	} else if (subject == CONTEXT_TRIGGER_SUBJECT_GET_RULE_IDS) {
		get_rule_ids(request);
	} else {
		_E("Invalid request");
	}
}

void ctx::context_trigger::push_fact(int req_id, int error, const char* subject, ctx::json& option, ctx::json& data, const char* zone)
{
	context_fact *fact = new(std::nothrow) context_fact(req_id, error, subject, option, data, zone);
	IF_FAIL_VOID_TAG(fact, _E, "Memory allocation failed");

	push_thread_event(ETYPE_FACT, fact);
}

void ctx::context_trigger::process_fact(ctx::context_fact* fact)
{
	// Process the context fact.
	rule_mgr->on_event_received(fact->get_subject(), fact->get_option(), fact->get_data(), fact->get_zone_name());
}

void ctx::context_trigger::process_initialize(void)
{
	rule_mgr = new(std::nothrow) rule_manager();
	IF_FAIL_VOID_TAG(rule_mgr, _E, "Memory allocation failed");

	bool ret = rule_mgr->init(this, _reader);
	if (!ret) {
		_E("Context trigger initialization failed.");
		raise(SIGTERM);
	}
}

void ctx::context_trigger::add_rule(ctx::request_info* request)
{
	ctx::json rule_id;

	const char* app_id = request->get_app_id();
	if (app_id == NULL) {
		request->reply(ERR_OPERATION_FAILED);
		return;
	}

	const char* zone_name = request->get_zone_name();
	if (zone_name == NULL) {
		request->reply(ERR_OPERATION_FAILED);
		return;
	}

	int error = rule_mgr->add_rule(app_id, request->get_description(), zone_name, &rule_id);
	_I("'%s' adds a rule (Error: %#x)", request->get_client(), error);

	request->reply(error, rule_id);
}

void ctx::context_trigger::remove_rule(ctx::request_info* request)
{
	int id;
	int error;

	const char* app_id = request->get_app_id();
	if (app_id == NULL) {
		request->reply(ERR_OPERATION_FAILED);
		return;
	}

	ctx::json rule_id = request->get_description();
	rule_id.get(NULL, CT_RULE_ID, &id);

	error = rule_mgr->check_rule(app_id, id);
	if (error != ERR_NONE) {
		request->reply(error);
		return;
	}

	bool ret = rule_mgr->is_rule_enabled(id);
	if (ret) {
		request->reply(ERR_RULE_ENABLED);
		return;
	}

	error = rule_mgr->remove_rule(id);
	_I("'%s' removes rule%d (Error: %#x)", request->get_client(), id, error);
	request->reply(error);
}

void ctx::context_trigger::enable_rule(ctx::request_info* request)
{
	int id;
	int error;

	const char* app_id = request->get_app_id();
	if (app_id == NULL) {
		request->reply(ERR_OPERATION_FAILED);
		return;
	}

	ctx::json rule_id = request->get_description();
	rule_id.get(NULL, CT_RULE_ID, &id);

	error = rule_mgr->check_rule(app_id, id);
	if (error != ERR_NONE) {
		request->reply(error);
		return;
	}

	bool ret = rule_mgr->is_rule_enabled(id);
	if (ret) {
		request->reply(ERR_RULE_ENABLED);
		return;
	}

	error = rule_mgr->enable_rule(id);
	_I("'%s' enables rule%d (Error: %#x)", request->get_client(), id, error);
	request->reply(error);
}

void ctx::context_trigger::disable_rule(ctx::request_info* request)
{
	int id;
	int error;

	const char* app_id = request->get_app_id();
	if (app_id == NULL) {
		request->reply(ERR_OPERATION_FAILED);
		return;
	}

	ctx::json rule_id = request->get_description();
	rule_id.get(NULL, CT_RULE_ID, &id);

	error = rule_mgr->check_rule(app_id, id);
	if (error != ERR_NONE) {
		request->reply(error);
		return;
	}

	bool ret = rule_mgr->is_rule_enabled(id);
	if (!ret) {
		request->reply(ERR_RULE_NOT_ENABLED);
		return;
	}

	error = rule_mgr->disable_rule(id);
	_I("'%s' disables rule%d (Error: %#x)", request->get_client(), id, error);
	request->reply(error);
}

void ctx::context_trigger::get_rule_by_id(ctx::request_info* request)
{
	int error;

	ctx::json option = request->get_description();
	int id;
	option.get(NULL, CT_RULE_ID, &id);

	const char* app_id = request->get_app_id();
	if (app_id == NULL) {
		request->reply(ERR_OPERATION_FAILED);
		return;
	}

	ctx::json read_data;
	error = rule_mgr->get_rule_by_id(app_id, id, &read_data);

	ctx::json dummy;
	request->reply(error, dummy, read_data);
}

void ctx::context_trigger::get_rule_ids(ctx::request_info* request)
{
	int error;

	const char* app_id = request->get_app_id();
	if (app_id == NULL) {
		request->reply(ERR_OPERATION_FAILED);
		return;
	}

	ctx::json read_data;
	error = rule_mgr->get_rule_ids(app_id, &read_data);

	ctx::json dummy;
	request->reply(error, dummy, read_data);
}
