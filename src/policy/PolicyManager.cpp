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
#include "../ContextManager.h"
#include "PolicyRequest.h"
#include "PolicyManager.h"

using namespace ctx;

PolicyManager::PolicyManager(ContextManager *contextMgr) :
	__contextMgr(contextMgr)
{
	__init();
}

PolicyManager::~PolicyManager()
{
	__release();
}

void PolicyManager::__init()
{
	/* TODO: WiFi API has multi-session support issue.
	   The issue should be fixed, or walkarouned first. */
	__subscribe(SUBJ_STATE_WIFI, __ridWifiState);
	__subscribe(SUBJ_APP_LOGGER, __ridAppLogging);
	__subscribe(SUBJ_MEDIA_LOGGER, __ridMediaLogging);

#if 0
	__subscribe(SUBJ_PLACE_DETECTION, __ridPlaceDetection);
#endif
}

void PolicyManager::__release()
{
	__unsubscribe(SUBJ_STATE_WIFI, __ridWifiState);
	__unsubscribe(SUBJ_APP_LOGGER, __ridAppLogging);
	__unsubscribe(SUBJ_MEDIA_LOGGER, __ridMediaLogging);

#if 0
	__unsubscribe(SUBJ_PLACE_DETECTION, __ridPlaceDetection);
#endif
}

void PolicyManager::__subscribe(const char *subject, int &reqId)
{
	static int rid = 0;
	++rid;

	reqId = rid;

	PolicyRequest *req = new(std::nothrow) PolicyRequest(REQ_SUBSCRIBE, reqId, subject, NULL);
	IF_FAIL_VOID_TAG(req, _E, "Memory allocation failed");

	__contextMgr->assignRequest(req);
}

void PolicyManager::__unsubscribe(const char *subject, int reqId)
{
	PolicyRequest *req = new(std::nothrow) PolicyRequest(REQ_UNSUBSCRIBE, reqId, subject, NULL);
	IF_FAIL_VOID_TAG(req, _E, "Memory allocation failed");

	__contextMgr->assignRequest(req);
}
