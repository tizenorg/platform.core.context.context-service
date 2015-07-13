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
#include "context_monitor.h"
#include "fact_reader.h"
#include "timer_types.h"

static ctx::fact_reader *reader = NULL;
typedef std::map<std::string, ctx::trigger_timer> timer_map_t;
static timer_map_t timer_map;	// <zone_name, timer>

ctx::context_monitor::context_monitor()
{
}

ctx::context_monitor::~context_monitor()
{
}

bool ctx::context_monitor::init(ctx::fact_reader* fr, ctx::context_trigger* tr)
{
	reader = fr;
	trigger = tr;

	return true;
}

int ctx::context_monitor::subscribe(int rule_id, std::string subject, ctx::json event, const char* zone)
{
	if (subject.compare(TIMER_EVENT_SUBJECT) == 0) {
		// option is event json in case of ON_TIME
		return subscribe_timer(event, zone);
	}

	ctx::json eoption = NULL;
	event.get(NULL, CT_RULE_EVENT_OPTION, &eoption);

	int req_id = reader->subscribe(subject.c_str(), &eoption, zone, true);
	IF_FAIL_RETURN_TAG(req_id > 0, ERR_OPERATION_FAILED, _E, "Subscribe event failed");
	_D(YELLOW("Subscribe event(rule%d). req%d"), rule_id, req_id);
	request_map[rule_id] = req_id;
	read_req_cnt_map[req_id]++;

	return ERR_NONE;
}

int ctx::context_monitor::unsubscribe(int rule_id, std::string subject, ctx::json option, const char* zone)
{
	if (subject.compare(TIMER_EVENT_SUBJECT) == 0) {
		return unsubscribe_timer(option, zone);
	}

	_D(YELLOW("Unsubscribe event(rule%d). req%d"), rule_id, request_map[rule_id]);
	int req_id = request_map[rule_id];
	request_map.erase(rule_id);

	read_req_cnt_map[req_id]--;
	if (read_req_cnt_map[req_id] == 0) {
		reader->unsubscribe(req_id);
		read_req_cnt_map.erase(req_id);
	}

	return ERR_NONE;
}

int ctx::context_monitor::read_time(ctx::json* result, const char* zone)
{
	int dom = ctx::trigger_timer::get_day_of_month();
	(*result).set(NULL, TIMER_RESPONSE_KEY_DAY_OF_MONTH, dom);

	std::string dow = ctx::trigger_timer::get_day_of_week();
	(*result).set(NULL, TIMER_RESPONSE_KEY_DAY_OF_WEEK, dow);

	int time = ctx::trigger_timer::get_minute_of_day();
	(*result).set(NULL, TIMER_RESPONSE_KEY_TIME_OF_DAY, time);

	return ERR_NONE;
}

int ctx::context_monitor::read(std::string subject, json option, const char* zone, ctx::json* result)
{
	bool ret;
	if (subject.compare(TIMER_CONDITION_SUBJECT) == 0) {
		return read_time(result, zone);
	}

	context_fact fact;
	ret = reader->read(subject.c_str(), &option, zone, fact);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Read fact failed");

	*result = fact.get_data();

	return ERR_NONE;
}

static int arrange_day_of_week(ctx::json day_info)
{
	int result = 0;

	std::string key_op;
	if (!day_info.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &key_op)) {
		result = ctx::trigger_timer::convert_string_to_day_of_week("\"" TIMER_EVERYDAY "\"");
		return result;
	}

	if (key_op.compare("and") == 0) {
		result = ctx::trigger_timer::convert_string_to_day_of_week("\"" TIMER_EVERYDAY "\"");
	}

	std::string tmp_d;
	for (int i = 0; day_info.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, i, &tmp_d); i++) {
		int dow = ctx::trigger_timer::convert_string_to_day_of_week(tmp_d);
		std::string op;
		day_info.get_array_elem(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, i, &op);

		if (op.compare("neq") == 0) {
			dow = ctx::trigger_timer::convert_string_to_day_of_week("\"" TIMER_EVERYDAY "\"") & ~dow;
		}

		if (key_op.compare("and") == 0) {
			result &= dow;
		} else {
			result |= dow;
		}
	}
	_D("Requested day of week (%#x)", result);

	return result;
}

timer_map_t::iterator ctx::context_monitor::get_zone_timer(std::string zone)
{
	timer_map_t::iterator it = timer_map.find(zone);

	if (it == timer_map.end()) {
		timer_map.insert(std::pair<std::string, ctx::trigger_timer>(zone, ctx::trigger_timer(trigger, zone)));
	}

	return timer_map.find(zone);
}

int ctx::context_monitor::subscribe_timer(ctx::json option, const char* zone)
{
	ctx::json day_info;
	ctx::json time_info;

	ctx::json it;
	for (int i = 0; option.get_array_elem(NULL, CT_RULE_DATA_ARR, i, &it); i++){
		std::string key;
		it.get(NULL, CT_RULE_DATA_KEY, &key);

		if (key.compare(TIMER_RESPONSE_KEY_DAY_OF_WEEK) == 0) {
			day_info = it;
		} else if (key.compare(TIMER_RESPONSE_KEY_TIME_OF_DAY) == 0) {
			time_info = it;
		}
	}

	// Day option processing
	int dow = arrange_day_of_week(day_info);

	// Time option processing
	int time; // minute
	timer_map_t::iterator timer = get_zone_timer(zone);
	for (int i = 0; time_info.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, i, &time); i++) {
		(timer->second).add(time, dow);
	}

	return ERR_NONE;
}

int ctx::context_monitor::unsubscribe_timer(ctx::json option, const char* zone)
{
	ctx::json day_info;
	ctx::json time_info;

	ctx::json it;
	for (int i = 0; option.get_array_elem(NULL, CT_RULE_DATA_ARR, i, &it); i++){
		std::string key;
		it.get(NULL, CT_RULE_DATA_KEY, &key);

		if (key.compare(TIMER_RESPONSE_KEY_DAY_OF_WEEK) == 0) {
			day_info = it;
		} else if (key.compare(TIMER_RESPONSE_KEY_TIME_OF_DAY) == 0) {
			time_info = it;
		}
	}

	// Day option processing
	int dow = arrange_day_of_week(day_info);

	// Time option processing
	int time; // minute
	timer_map_t::iterator timer = get_zone_timer(zone);
	for (int i = 0; time_info.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, i, &time); i++) {
		(timer->second).remove(time, dow);
	}

	if ((timer->second).empty()) {
		timer_map.erase(timer);
	}

	return ERR_NONE;
}

bool ctx::context_monitor::is_supported(std::string subject, const char* zone)
{
	if (subject.compare(TIMER_EVENT_SUBJECT) == 0
			|| subject.compare(TIMER_CONDITION_SUBJECT) == 0) {
		return true;
	}

	return reader->is_supported(subject.c_str(), zone);
}
