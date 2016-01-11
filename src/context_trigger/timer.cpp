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

#include <time.h>
#include <types_internal.h>
#include <scope_mutex.h>
#include <timer_mgr.h>
#include <context_mgr.h>
#include <context_trigger_types_internal.h>
#include "timer.h"
#include "timer_types.h"
#include "context_listener_iface.h"

#define MAX_HOUR	24
#define MAX_DAY		7

using namespace ctx::timer_manager;
static GMutex timer_mutex;

static int convert_string_to_day_of_week(std::string d)
{
	int day = 0;

	if (d.compare(TIMER_SUN) == 0) {
		day = SUN;
	} else if (d.compare(TIMER_MON) == 0) {
		day = MON;
	} else if (d.compare(TIMER_TUE) == 0) {
		day = TUE;
	} else if (d.compare(TIMER_WED) == 0) {
		day = WED;
	} else if (d.compare(TIMER_THU) == 0) {
		day = THU;
	} else if (d.compare(TIMER_FRI) == 0) {
		day = FRI;
	} else if (d.compare(TIMER_SAT) == 0) {
		day = SAT;
	} else if (d.compare(TIMER_WEEKDAY) == 0) {
		day = WEEKDAY;
	} else if (d.compare(TIMER_WEEKEND) == 0) {
		day = WEEKEND;
	} else if (d.compare(TIMER_EVERYDAY) == 0) {
		day = EVERYDAY;
	}

	return day;
}

static int arrange_day_of_week(ctx::json day_info)
{
	int result = 0;

	std::string key_op;
	if (!day_info.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &key_op)) {
		result = convert_string_to_day_of_week(TIMER_EVERYDAY);
		return result;
	}

	if (key_op.compare("and") == 0) {
		result = convert_string_to_day_of_week(TIMER_EVERYDAY);
	}

	std::string tmp_d;
	for (int i = 0; day_info.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, i, &tmp_d); i++) {
		int dow = convert_string_to_day_of_week(tmp_d);
		std::string op;
		day_info.get_array_elem(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, i, &op);

		if (op.compare("neq") == 0) {
			dow = convert_string_to_day_of_week(TIMER_EVERYDAY) & ~dow;
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

static std::string convert_day_of_week_to_string(int d)
{
	std::string day;

	if (d == SUN) {
		day = TIMER_SUN;
	} else if (d == MON) {
		day = TIMER_MON;
	} else if (d == TUE) {
		day = TIMER_TUE;
	} else if (d == WED) {
		day = TIMER_WED;
	} else if (d == THU) {
		day = TIMER_THU;
	} else if (d == FRI) {
		day = TIMER_FRI;
	} else if (d == SAT) {
		day = TIMER_SAT;
	} else if (d == WEEKDAY) {
		day = TIMER_WEEKDAY;
	} else if (d == WEEKEND) {
		day = TIMER_WEEKEND;
	} else if (d == EVERYDAY) {
		day = TIMER_EVERYDAY;
	}

	return day;
}

ctx::trigger_timer::ref_count_array_s::ref_count_array_s()
{
	memset(count, 0, sizeof(int) * MAX_DAY);
}

ctx::trigger_timer::trigger_timer()
{
	submit_trigger_item();
}

ctx::trigger_timer::~trigger_timer()
{
	clear();
}

void ctx::trigger_timer::submit_trigger_item()
{
	context_manager::register_trigger_item(TIMER_EVENT_SUBJECT, OPS_SUBSCRIBE,
			"{"
				"\"TimeOfDay\":{\"type\":\"integer\",\"min\":0,\"max\":1439},"
				"\"DayOfWeek\":{\"type\":\"string\",\"values\":[\"Mon\",\"Tue\",\"Wed\",\"Thu\",\"Fri\",\"Sat\",\"Sun\",\"Weekday\",\"Weekend\"]}"
			"}",
			NULL);

	context_manager::register_trigger_item(TIMER_CONDITION_SUBJECT, OPS_READ,
			"{"
				"\"TimeOfDay\":{\"type\":\"integer\",\"min\":0,\"max\":1439},"
				"\"DayOfWeek\":{\"type\":\"string\",\"values\":[\"Mon\",\"Tue\",\"Wed\",\"Thu\",\"Fri\",\"Sat\",\"Sun\",\"Weekday\",\"Weekend\"]},"
				"\"DayOfMonth\":{\"type\":\"integer\",\"min\":1,\"max\":31}"
			"}",
			NULL);

}

int ctx::trigger_timer::merge_day_of_week(int* ref_cnt)
{
	int day_of_week = 0;

	for (int d = 0; d < MAX_DAY; ++d) {
		if (ref_cnt[d] > 0) {
			day_of_week |= (0x01 << d);
		}
	}

	return day_of_week;
}

bool ctx::trigger_timer::add(int minute, int day_of_week)
{
	IF_FAIL_RETURN_TAG(minute >=0 && minute < 1440 &&
			day_of_week > 0 && day_of_week <= timer_manager::EVERYDAY,
			false, _E, "Invalid parameter");

	ctx::scope_mutex sm(&timer_mutex);

	ref_count_array_s &ref = ref_count_map[minute];

	for (int d = 0; d < MAX_DAY; ++d) {
		if ((day_of_week & (0x01 << d)) != 0) {
			ref.count[d] += 1;
		}
	}

	return reset_timer(minute);
}

bool ctx::trigger_timer::remove(int minute, int day_of_week)
{
	IF_FAIL_RETURN_TAG(minute >=0 && minute < 1440 &&
			day_of_week > 0 && day_of_week <= timer_manager::EVERYDAY,
			false, _E, "Invalid parameter");

	ctx::scope_mutex sm(&timer_mutex);

	ref_count_array_s &ref = ref_count_map[minute];

	for (int d = 0; d < MAX_DAY; ++d) {
		if ((day_of_week & (0x01 << d)) != 0 && ref.count[d] > 0) {
			ref.count[d] -= 1;
		}
	}

	return reset_timer(minute);
}

bool ctx::trigger_timer::reset_timer(int minute)
{
	int day_of_week = merge_day_of_week(ref_count_map[minute].count);
	timer_state_s &timer = timer_state_map[minute];

	if (day_of_week == timer.day_of_week) {
		/* Necessary timers are already running... */
		return true;
	}

	if (day_of_week == 0 && timer.timer_id > 0) {
		/* Turn off the timer at hour, if it is not necessray anymore. */
		timer_manager::remove(timer.timer_id);
		timer_state_map.erase(minute);
		ref_count_map.erase(minute);
		return true;
	}

	if (timer.timer_id > 0) {
		/* Turn off the current timer, to set a new one. */
		timer_manager::remove(timer.timer_id);
		timer.timer_id = -1;
		timer.day_of_week = 0;
	}

	/* Create a new timer, w.r.t. the new day_of_week value. */
	int h = minute / 60;
	int m = minute - h * 60;
	int tid = timer_manager::set_at(h, m, day_of_week, this);
	IF_FAIL_RETURN_TAG(tid > 0, false, _E, "Timer setting failed");

	timer.timer_id = tid;
	timer.day_of_week = day_of_week;

	return true;
}

void ctx::trigger_timer::clear()
{
	ctx::scope_mutex sm(&timer_mutex);

	for (timer_state_map_t::iterator it = timer_state_map.begin(); it != timer_state_map.end(); ++it) {
		if (it->second.timer_id > 0) {
			timer_manager::remove(it->second.timer_id);
		}
	}

	timer_state_map.clear();
	ref_count_map.clear();
}

bool ctx::trigger_timer::on_timer_expired(int timer_id, void* user_data)
{
	time_t rawtime;
	struct tm timeinfo;

	time(&rawtime);
	tzset();
	localtime_r(&rawtime, &timeinfo);

	int hour = timeinfo.tm_hour;
	int min = timeinfo.tm_min;
	int day_of_week = (0x01 << timeinfo.tm_wday);

	on_timer_expired(hour, min, day_of_week);

	return true;
}

void ctx::trigger_timer::on_timer_expired(int hour, int min, int day_of_week)
{
	_I("Time: %02d:%02d, Day of Week: %#x", hour, min, day_of_week);

	ctx::json result;
	result.set(NULL, TIMER_RESPONSE_KEY_TIME_OF_DAY, hour * 60 + min);
	result.set(NULL, TIMER_RESPONSE_KEY_DAY_OF_WEEK, convert_day_of_week_to_string(day_of_week));

	ctx::json dummy = NULL;

	for (listener_list_t::iterator it = listener_list.begin(); it != listener_list.end(); ++it) {
		(*it)->on_event_received(TIMER_EVENT_SUBJECT, EMPTY_JSON_OBJECT, result);
	}
}

int ctx::trigger_timer::subscribe(ctx::json option, context_listener_iface* listener)
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
	for (int i = 0; time_info.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, i, &time); i++) {
		add(time, dow);
	}

	listener_list.push_back(listener);

	return ERR_NONE;
}

int ctx::trigger_timer::unsubscribe(ctx::json option, context_listener_iface* listener)
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
	for (int i = 0; time_info.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, i, &time); i++) {
		remove(time, dow);
	}

	listener_list.remove(listener);

	return ERR_NONE;
}

int ctx::trigger_timer::read(context_listener_iface* listener)
{
	time_t rawtime;
	struct tm timeinfo;

	time(&rawtime);
	tzset();
	localtime_r(&rawtime, &timeinfo);

	int day_of_month = timeinfo.tm_mday;
	int minute_of_day = timeinfo.tm_hour * 60 + timeinfo.tm_min;
	std::string day_of_week = convert_day_of_week_to_string(0x01 << timeinfo.tm_wday);

	ctx::json result;
	result.set(NULL, TIMER_RESPONSE_KEY_DAY_OF_MONTH, day_of_month);
	result.set(NULL, TIMER_RESPONSE_KEY_DAY_OF_WEEK, day_of_week);
	result.set(NULL, TIMER_RESPONSE_KEY_TIME_OF_DAY, minute_of_day);

	_I("Time: %02d:%02d, Day of Week: %s, Day of Month: %d", timeinfo.tm_hour, timeinfo.tm_min, day_of_week.c_str(), day_of_month);

	listener->on_condition_received(TIMER_CONDITION_SUBJECT, EMPTY_JSON_OBJECT, result);

	return ERR_NONE;
}

bool ctx::trigger_timer::empty()
{
	return timer_state_map.empty();
}

void ctx::trigger_timer::handle_timer_event(ctx::json& rule)
{
	ctx::json event;
	rule.get(NULL, CT_RULE_EVENT, &event);

	std::string e_name;
	event.get(NULL, CT_RULE_EVENT_ITEM, &e_name);
	if (e_name.compare(TIMER_EVENT_SUBJECT) != 0 ) {
		return;
	}

	ctx::json day_info;
	ctx::json it;
	int dow;
	for (int i = 0; event.get_array_elem(NULL, CT_RULE_DATA_ARR, i, &it); i++){
		std::string key;
		it.get(NULL, CT_RULE_DATA_KEY, &key);

		if (key.compare(TIMER_RESPONSE_KEY_DAY_OF_WEEK) == 0) {
			dow = arrange_day_of_week(it);

			day_info.set(NULL, CT_RULE_DATA_KEY, TIMER_RESPONSE_KEY_DAY_OF_WEEK);
			day_info.set(NULL, CT_RULE_DATA_KEY_OPERATOR, "or");

			for (int j = 0; j < MAX_DAY; j++) {
				int d = 0x01 << j;
				if (dow & d) {
					std::string day = convert_day_of_week_to_string(d);
					day_info.array_append(NULL, CT_RULE_DATA_VALUE_ARR, day);
					day_info.array_append(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, "eq");
				}
			}
			event.array_set_at(NULL, CT_RULE_DATA_ARR, i, day_info);
		}

	}

	rule.set(NULL, CT_RULE_EVENT, event);
}
