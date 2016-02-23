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

#include <context_trigger.h>
#include <context_trigger_types_internal.h>
#include <types_internal.h>
#include <timer_util.h>
#include "timer.h"

#define TIMER_DAY_OF_WEEK "DayOfWeek"
#define TIMER_TIME_OF_DAY "TimeOfDay"

static int arrange_day_of_week(ctx::Json day_info)
{
	int result = 0;

	std::string key_op;
	if (!day_info.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &key_op)) {
		result = ctx::timer_util::convert_day_of_week_string_to_int(TIMER_TYPES_EVERYDAY);
		return result;
	}

	if (key_op.compare("and") == 0) {
		result = ctx::timer_util::convert_day_of_week_string_to_int(TIMER_TYPES_EVERYDAY);
	}

	std::string tmp_d;
	for (int i = 0; day_info.getAt(NULL, CT_RULE_DATA_VALUE_ARR, i, &tmp_d); i++) {
		int dow = ctx::timer_util::convert_day_of_week_string_to_int(tmp_d);
		std::string op;
		day_info.getAt(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, i, &op);

		if (op.compare(CONTEXT_TRIGGER_NOT_EQUAL_TO) == 0) {
			dow = ctx::timer_util::convert_day_of_week_string_to_int(TIMER_TYPES_EVERYDAY) & ~dow;
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

void ctx::trigger_timer::handle_timer_event(ctx::Json& rule)
{
	ctx::Json event;
	rule.get(NULL, CT_RULE_EVENT, &event);

	std::string e_name;
	event.get(NULL, CT_RULE_EVENT_ITEM, &e_name);
	if (e_name.compare(CT_EVENT_TIME) != 0 ) {
		return;
	}

	ctx::Json day_info;
	ctx::Json it;
	int dow;
	for (int i = 0; event.getAt(NULL, CT_RULE_DATA_ARR, i, &it); i++) {
		std::string key;
		it.get(NULL, CT_RULE_DATA_KEY, &key);

		if (key.compare(TIMER_DAY_OF_WEEK) == 0) {
			dow = arrange_day_of_week(it);

			day_info.set(NULL, CT_RULE_DATA_KEY, TIMER_DAY_OF_WEEK);
			day_info.set(NULL, CT_RULE_DATA_KEY_OPERATOR, "or");

			for (int j = 0; j < MAX_DAY; j++) {
				int d = 0x01 << j;
				if (dow & d) {
					std::string day = ctx::timer_util::convert_day_of_week_int_to_string(d);
					day_info.append(NULL, CT_RULE_DATA_VALUE_ARR, day);
					day_info.append(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, CONTEXT_TRIGGER_EQUAL_TO);

					// Set option
					event.append(CT_RULE_EVENT_OPTION, TIMER_DAY_OF_WEEK, day);
				}
			}
			event.setAt(NULL, CT_RULE_DATA_ARR, i, day_info);
		} else if (key.compare(TIMER_TIME_OF_DAY) == 0) {
			int time;
			for (int j = 0; it.getAt(NULL, CT_RULE_DATA_VALUE_ARR, j, &time); j++) {
				event.append(CT_RULE_EVENT_OPTION, TIMER_TIME_OF_DAY, time);
			}
		}
	}

	rule.set(NULL, CT_RULE_EVENT, event);
}
