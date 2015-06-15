/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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

#include <map>
#include <alarm.h>
#include <scope_mutex.h>
#include <timer_mgr.h>
#include "timer_mgr_impl.h"

#define IDENTIFIER "contextd"

struct listener_info_s {
	int timer_id;
	ctx::timer_listener_iface* listener;
	void* user_data;
};

typedef std::map<alarm_id_t, listener_info_s> listener_map_t;
static listener_map_t *listener_map = NULL;
static GMutex listener_map_mutex;

static int generate_timer_id()
{
	static int tid = 0;

	tid ++;
	if (tid < 0) {
		_W("Overflow");
		tid = 1;
	}

	return tid;
}

static int alarm_expired_cb(alarm_id_t alarm_id, void* cb_data)
{
	int timer_id = 0;
	ctx::timer_listener_iface *listener = NULL;
	void* user_data;

	{
		ctx::scope_mutex sm1(&listener_map_mutex);
		listener_map_t::iterator mit = listener_map->find(alarm_id);
		IF_FAIL_RETURN_TAG(mit != listener_map->end(), 0, _W, "Unknown Alarm %d", alarm_id);
		timer_id = mit->second.timer_id;
		listener = mit->second.listener;
		user_data = mit->second.user_data;
	}

	_D("Timer %d expired", timer_id);
	bool repeat = listener->on_timer_expired(timer_id, user_data);

	if (!repeat) {
		_D("Stop repeating Timer %d (Alarm %d)", timer_id, alarm_id);
		ctx::scope_mutex sm2(&listener_map_mutex);
		alarmmgr_remove_alarm(alarm_id);
		listener_map->erase(alarm_id);
	}

	return 0;
}

ctx::timer_manager_impl::timer_manager_impl()
	: initialized(false)
{
}

ctx::timer_manager_impl::~timer_manager_impl()
{
	release();
}

bool ctx::timer_manager_impl::init()
{
	IF_FAIL_RETURN_TAG(!initialized, true, _W, "Re-initialization");

	listener_map = new(std::nothrow) listener_map_t;
	IF_FAIL_RETURN_TAG(listener_map, false, _E, "Memory allocation failed");

	int result = alarmmgr_init(IDENTIFIER);
	IF_FAIL_RETURN_TAG(result == ALARMMGR_RESULT_SUCCESS, false, _E, "Alarm manager initialization failed");

	result = alarmmgr_set_cb(alarm_expired_cb, this);
	if (result != ALARMMGR_RESULT_SUCCESS) {
		alarmmgr_fini();
		_E("Alarm callback registration failed");
		return false;
	}

	alarmmgr_remove_all();
	initialized = true;
	return true;
}

void ctx::timer_manager_impl::release()
{
	if (initialized) {
		alarmmgr_remove_all();
		alarmmgr_fini();
		delete listener_map;
		listener_map = NULL;
		initialized = false;
	}
}

int ctx::timer_manager_impl::set_for(int interval, timer_listener_iface* listener, void* user_data)
{
	IF_FAIL_RETURN_TAG(interval > 0 && listener, false, _E, "Invalid parameter");

	alarm_id_t alarm_id;
	int result;

#if 0
	// Implementation for Tizen 2.3
	time_t trigger_time;
	time(&trigger_time);
	// time_t is in seconds.. It is the POSIX specification.
	trigger_time += (interval * 60);

	result = alarmmgr_add_alarm(ALARM_TYPE_VOLATILE, trigger_time, interval * 60, NULL, &alarm_id);
#endif

	result = alarmmgr_add_periodic_alarm_withcb(interval, QUANTUMIZE, alarm_expired_cb, this, &alarm_id);
	IF_FAIL_RETURN_TAG(result == ALARMMGR_RESULT_SUCCESS, ERR_OPERATION_FAILED, _E, "Alarm initialization failed");

	ctx::scope_mutex sm(&listener_map_mutex);

	listener_info_s info;
	info.timer_id = generate_timer_id();
	info.listener = listener;
	info.user_data = user_data;
	(*listener_map)[alarm_id] = info;

	_D("Timer %d was set for %dm interval", info.timer_id, interval);

	return info.timer_id;
}

int ctx::timer_manager_impl::set_at(int hour, int min, int day_of_week, timer_listener_iface* listener, void* user_data)
{
	IF_FAIL_RETURN_TAG(
			hour < 24 && hour >= 0 &&
			min < 60 && min >= 0 &&
			day_of_week > 0 && day_of_week <= timer_manager::EVERYDAY &&
			listener, false, _E, "Invalid parameter");

	int repeat = 0;
	if (day_of_week & timer_manager::SUN) repeat |= ALARM_WDAY_SUNDAY;
	if (day_of_week & timer_manager::MON) repeat |= ALARM_WDAY_MONDAY;
	if (day_of_week & timer_manager::TUE) repeat |= ALARM_WDAY_TUESDAY;
	if (day_of_week & timer_manager::WED) repeat |= ALARM_WDAY_WEDNESDAY;
	if (day_of_week & timer_manager::THU) repeat |= ALARM_WDAY_THURSDAY;
	if (day_of_week & timer_manager::FRI) repeat |= ALARM_WDAY_FRIDAY;
	if (day_of_week & timer_manager::SAT) repeat |= ALARM_WDAY_SATURDAY;

	alarm_entry_t *alarm_info = alarmmgr_create_alarm();
	IF_FAIL_RETURN_TAG(alarm_info, ERR_OPERATION_FAILED, _E, "Memory allocation failed");

	time_t current_time;
	struct tm current_tm;
	time(&current_time);
	tzset();
	localtime_r(&current_time, &current_tm);

	alarm_date_t alarm_time;
	alarm_time.year = current_tm.tm_year + 1900;
	alarm_time.month = current_tm.tm_mon + 1;
	alarm_time.day = current_tm.tm_mday;
	alarm_time.hour = hour;
	alarm_time.min = min;
	alarm_time.sec = 0;

	alarmmgr_set_time(alarm_info, alarm_time);
	alarmmgr_set_repeat_mode(alarm_info, ALARM_REPEAT_MODE_WEEKLY, repeat);
	alarmmgr_set_type(alarm_info, ALARM_TYPE_VOLATILE);

	alarm_id_t alarm_id;
	int ret = alarmmgr_add_alarm_with_localtime(alarm_info, NULL, &alarm_id);
	alarmmgr_free_alarm(alarm_info);

	IF_FAIL_RETURN_TAG(ret == ALARMMGR_RESULT_SUCCESS, ERR_OPERATION_FAILED, _E, "Alarm initialization failed");

	ctx::scope_mutex sm(&listener_map_mutex);

	listener_info_s info;
	info.timer_id = generate_timer_id();
	info.listener = listener;
	info.user_data = user_data;

	(*listener_map)[alarm_id] = info;

	_D("Timer %d was set at %02d:%02d:00 (day of week: %#x)", info.timer_id, hour, min, day_of_week);

	return info.timer_id;
}

void ctx::timer_manager_impl::remove(int timer_id)
{
	ctx::scope_mutex sm(&listener_map_mutex);

	listener_map_t::iterator it;
	for (it = listener_map->begin(); it != listener_map->end(); ++it) {
		if (it->second.timer_id == timer_id) {
			alarmmgr_remove_alarm(it->first);
			listener_map->erase(it);
			_D("Timer %d was removed", timer_id);
			break;
		}
	}
}
