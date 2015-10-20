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
#include <json.h>
#include <context_trigger_types_internal.h>
#include "../access_control/privilege.h"
#include "context_monitor.h"
#include "fact_reader.h"
#include "timer_types.h"

static ctx::fact_reader *reader = NULL;

ctx::context_monitor::context_monitor()
{
}

ctx::context_monitor::~context_monitor()
{
	delete timer;
}

bool ctx::context_monitor::init(ctx::fact_reader* fr, ctx::context_trigger* tr)
{
	reader = fr;
	trigger = tr;
	timer = new(std::nothrow) trigger_timer(trigger);
	IF_FAIL_RETURN_TAG(timer, false, _E, "Memory allocation failed");

	return true;
}

int ctx::context_monitor::subscribe(int rule_id, std::string subject, ctx::json event)
{
	if (subject.compare(TIMER_EVENT_SUBJECT) == 0) {
		// option is event json in case of ON_TIME
		return timer->subscribe(event);
	}

	ctx::json eoption = NULL;
	event.get(NULL, CT_RULE_EVENT_OPTION, &eoption);

	int req_id = reader->subscribe(subject.c_str(), &eoption, true);
	IF_FAIL_RETURN_TAG(req_id > 0, ERR_OPERATION_FAILED, _E, "Subscribe event failed");
	_D(YELLOW("Subscribe event(rule%d). req%d"), rule_id, req_id);
	request_map[rule_id] = req_id;
	read_req_cnt_map[req_id]++;

	return ERR_NONE;
}

int ctx::context_monitor::unsubscribe(int rule_id, std::string subject, ctx::json option)
{
	if (subject.compare(TIMER_EVENT_SUBJECT) == 0) {
		return timer->unsubscribe(option);
	}

	_D(YELLOW("Unsubscribe event(rule%d). req%d"), rule_id, request_map[rule_id]);
	int req_id = request_map[rule_id];
	request_map.erase(rule_id);

	read_req_cnt_map[req_id]--;
	if (read_req_cnt_map[req_id] == 0) {
		reader->unsubscribe(subject.c_str(), req_id);
		read_req_cnt_map.erase(req_id);
	}

	return ERR_NONE;
}

int ctx::context_monitor::read(std::string subject, json option, ctx::json* result)
{
	bool ret;
	if (subject.compare(TIMER_CONDITION_SUBJECT) == 0) {
		return timer->read(result);
	}

	context_fact fact;
	ret = reader->read(subject.c_str(), &option, fact);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Read fact failed");

	*result = fact.get_data();

	return ERR_NONE;
}

bool ctx::context_monitor::is_supported(std::string subject)
{
	if (subject.compare(TIMER_EVENT_SUBJECT) == 0
			|| subject.compare(TIMER_CONDITION_SUBJECT) == 0) {
		return true;
	}

	return reader->is_supported(subject.c_str());
}

bool ctx::context_monitor::is_allowed(const char *client, const char *subject)
{
	if (STR_EQ(subject, TIMER_EVENT_SUBJECT))
		return privilege_manager::is_allowed(client, PRIV_ALARM_SET);

	if (STR_EQ(subject, TIMER_CONDITION_SUBJECT))
		return true;

	return reader->is_allowed(client, subject);
}
