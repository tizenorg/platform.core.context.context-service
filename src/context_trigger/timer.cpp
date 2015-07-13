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

#include <stdlib.h>
#include <time.h>
#include <types_internal.h>
#include <scope_mutex.h>
#include <timer_mgr.h>
#include "timer.h"
#include "trigger.h"
#include "timer_types.h"

#define MAX_HOUR	24
#define MAX_DAY		7

using namespace ctx::timer_manager;
static GMutex timer_mutex;

ctx::trigger_timer::ref_count_array_s::ref_count_array_s()
{
	memset(count, 0, sizeof(int) * MAX_DAY);
}

ctx::trigger_timer::trigger_timer(ctx::context_trigger* tr, std::string z)
	: trigger(tr)
	, zone(z)
{
}

ctx::trigger_timer::~trigger_timer()
{
	clear();
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
	_I("[Timer-%s] Time: %02d:%02d, Day of Week: %#x", zone.c_str(), hour, min, day_of_week);

	ctx::json result;
	result.set(NULL, TIMER_RESPONSE_KEY_TIME_OF_DAY, hour * 60 + min);
	result.set(NULL, TIMER_RESPONSE_KEY_DAY_OF_WEEK, convert_day_of_week_to_string(day_of_week));

	ctx::json dummy = NULL;
	trigger->push_fact(TIMER_EVENT_REQ_ID, ERR_NONE, TIMER_EVENT_SUBJECT, dummy, result, zone.c_str());
}

int ctx::trigger_timer::get_day_of_month()
{
	time_t rawtime;
	struct tm timeinfo;

	time(&rawtime);
	tzset();
	localtime_r(&rawtime, &timeinfo);

	int day_of_month = timeinfo.tm_mday;

	return day_of_month;
}

std::string ctx::trigger_timer::get_day_of_week()
{
	time_t rawtime;
	struct tm timeinfo;

	time(&rawtime);
	tzset();
	localtime_r(&rawtime, &timeinfo);

	int day_of_week = (0x01 << timeinfo.tm_wday);

	return convert_day_of_week_to_string(day_of_week);
}

int ctx::trigger_timer::get_minute_of_day()
{
	time_t rawtime;
	struct tm timeinfo;

	time(&rawtime);
	tzset();
	localtime_r(&rawtime, &timeinfo);

	int hour = timeinfo.tm_hour;
	int minute = timeinfo.tm_min;

	return hour * 60 + minute;
}

int ctx::trigger_timer::convert_string_to_day_of_week(std::string d)
{
	int day = 0;
	d = d.substr(1, d.length() - 2);

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

std::string ctx::trigger_timer::convert_day_of_week_to_string(int d)
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

bool ctx::trigger_timer::empty()
{
	return timer_state_map.empty();
}
