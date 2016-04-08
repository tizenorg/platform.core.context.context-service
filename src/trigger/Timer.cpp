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
#include <TimerManager.h>
#include "Timer.h"

#define TIMER_DAY_OF_WEEK "DayOfWeek"
#define TIMER_TIME_OF_DAY "TimeOfDay"

using namespace ctx;
using namespace ctx::trigger;

static int __arrangeDayOfWeek(Json dayInfo)
{
	int result = 0;

	std::string key_op;
	if (!dayInfo.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &key_op)) {
		result = TimerManager::dowToInt(DOW_EVERYDAY);
		return result;
	}

	if (key_op.compare("and") == 0) {
		result = TimerManager::dowToInt(DOW_EVERYDAY);
	}

	std::string tmp_d;
	for (int i = 0; dayInfo.getAt(NULL, CT_RULE_DATA_VALUE_ARR, i, &tmp_d); i++) {
		int dow = TimerManager::dowToInt(tmp_d);
		std::string op;
		dayInfo.getAt(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, i, &op);

		if (op.compare(CONTEXT_TRIGGER_NOT_EQUAL_TO) == 0) {
			dow = TimerManager::dowToInt(DOW_EVERYDAY) & ~dow;
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

void timer::handleTimerEvent(Json& rule)
{
	Json event;
	rule.get(NULL, CT_RULE_EVENT, &event);

	std::string eventName;
	event.get(NULL, CT_RULE_EVENT_ITEM, &eventName);
	if (eventName.compare(CT_EVENT_TIME) != 0) {
		return;
	}

	Json dayInfo;
	Json it;
	int dow;
	for (int i = 0; event.getAt(NULL, CT_RULE_DATA_ARR, i, &it); i++) {
		std::string key;
		it.get(NULL, CT_RULE_DATA_KEY, &key);

		if (key.compare(TIMER_DAY_OF_WEEK) == 0) {
			dow = __arrangeDayOfWeek(it);

			dayInfo.set(NULL, CT_RULE_DATA_KEY, TIMER_DAY_OF_WEEK);
			dayInfo.set(NULL, CT_RULE_DATA_KEY_OPERATOR, "or");

			for (int j = 0; j < DAYS_PER_WEEK; j++) {
				int d = 0x01 << j;
				if (dow & d) {
					std::string day = TimerManager::dowToStr(d);
					dayInfo.append(NULL, CT_RULE_DATA_VALUE_ARR, day);
					dayInfo.append(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, CONTEXT_TRIGGER_EQUAL_TO);

					// Set option
					event.append(CT_RULE_EVENT_OPTION, TIMER_DAY_OF_WEEK, day);
				}
			}
			event.setAt(NULL, CT_RULE_DATA_ARR, i, dayInfo);
		} else if (key.compare(TIMER_TIME_OF_DAY) == 0) {
			int time;
			for (int j = 0; it.getAt(NULL, CT_RULE_DATA_VALUE_ARR, j, &time); j++) {
				event.append(CT_RULE_EVENT_OPTION, TIMER_TIME_OF_DAY, time);
			}
		}
	}

	rule.set(NULL, CT_RULE_EVENT, event);
}
