/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#include <Types.h>
#include <DBusTypes.h>
#include <ProviderTypes.h>
#include <SensorRecorderTypes.h>
#include "../ContextManager.h"
#include "PolicyRequest.h"
#include "PolicyManager.h"

using namespace ctx;

PolicyManager::PolicyManager(ContextManager *contextMgr) :
	__contextMgr(contextMgr)
{
#ifdef _MOBILE_
	__subscribe(SUBJ_STATE_WIFI);
	__subscribe(SUBJ_APP_LOGGER);
	__subscribe(SUBJ_MEDIA_LOGGER);
	__subscribe(SUBJ_SENSOR_PEDOMETER);
	__subscribe(SUBJ_SENSOR_PRESSURE);
//	__subscribe(SUBJ_PLACE_DETECTION);
#endif

#ifdef _WEARABLE_
	__subscribe(SUBJ_SENSOR_PEDOMETER);
	__subscribe(SUBJ_SENSOR_PRESSURE);
	__subscribe(SUBJ_SENSOR_SLEEP_MONITOR);
#endif

#ifdef TRIGGER_SUPPORT
	__subscribe(SUBJ_CUSTOM);
#endif
}

PolicyManager::~PolicyManager()
{
	for (auto &it : __subscriptionMap) {
		PolicyRequest *req = new(std::nothrow) PolicyRequest(REQ_UNSUBSCRIBE, it.first, it.second, NULL);
		if (!req) {
			_E("Memory allocation failed");
			continue;
		}
		__contextMgr->assignRequest(req);
	}

	__subscriptionMap.clear();
}

void PolicyManager::__subscribe(const char *subject)
{
	static int rid = 0;
	++rid;

	PolicyRequest *req = new(std::nothrow) PolicyRequest(REQ_SUBSCRIBE, rid, subject, NULL);
	IF_FAIL_VOID_TAG(req, _E, "Memory allocation failed");

	__contextMgr->assignRequest(req);

	__subscriptionMap[rid] = subject;
}
