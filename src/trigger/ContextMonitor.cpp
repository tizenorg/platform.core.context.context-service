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

#include <Types.h>
#include "../access_control/Privilege.h"
#include "../ContextManagerImpl.h"
#include "ContextMonitor.h"
#include "IContextListener.h"
#include "FactRequest.h"

using namespace ctx;
using namespace trigger;

static int __lastRid;
static int __lastErr;

ContextMonitor *ContextMonitor::__instance = NULL;
ContextManagerImpl *ContextMonitor::__contextMgr = NULL;

static int __generateReqId()
{
	static int reqId = 0;

	if (++reqId < 0) {
		// Overflow handling
		reqId = 1;
	}

	return reqId;
}

ContextMonitor::ContextMonitor()
{
}

ContextMonitor::~ContextMonitor()
{
}

void ContextMonitor::setContextManager(ContextManagerImpl* ctx_mgr)
{
	__contextMgr = ctx_mgr;
}

ContextMonitor* ContextMonitor::getInstance()
{
	IF_FAIL_RETURN_TAG(__contextMgr, NULL, _E, "Context manager is needed");

	IF_FAIL_RETURN(!__instance, __instance);

	__instance = new(std::nothrow) ContextMonitor();
	IF_FAIL_RETURN_TAG(__instance, NULL, _E, "Memory alllocation failed");

	return __instance;
}

void ContextMonitor::destroy()
{
	if (__instance) {
		delete __instance;
		__instance = NULL;
	}
}

int ContextMonitor::subscribe(int ruleId, std::string subject, Json option, IContextListener* listener)
{
	int reqId = __subscribe(subject.c_str(), &option, listener);
	IF_FAIL_RETURN_TAG(reqId > 0, reqId, _E, "Subscribe event failed");
	_D(YELLOW("Subscribe event(rule%d). req%d"), ruleId, reqId);

	return ERR_NONE;
}

int ContextMonitor::__subscribe(const char* subject, Json* option, IContextListener* listener)
{
	IF_FAIL_RETURN(subject, ERR_INVALID_PARAMETER);

	int rid = __findSub(REQ_SUBSCRIBE, subject, option);
	if (rid > 0) {
		__addListener(REQ_SUBSCRIBE, rid, listener);
		_D("Duplicated request for %s", subject);
		return rid;
	}

	rid = __generateReqId();

	FactRequest *req = new(std::nothrow) FactRequest(REQ_SUBSCRIBE,
			rid, subject, option ? option->str().c_str() : NULL, this);
	IF_FAIL_RETURN_TAG(req, ERR_OUT_OF_MEMORY, _E, "Memory allocation failed");

	__contextMgr->assignRequest(req);
	__addSub(REQ_SUBSCRIBE, rid, subject, option, listener);

	if (__lastErr != ERR_NONE) {
		__removeSub(REQ_SUBSCRIBE, rid);
		_E("Subscription request failed: %#x", __lastErr);
		return __lastErr;
	}

	return rid;
}

int ContextMonitor::unsubscribe(int ruleId, std::string subject, Json option, IContextListener* listener)
{
	int rid = __findSub(REQ_SUBSCRIBE, subject.c_str(), &option);
	if (rid < 0) {
		_D("Invalid unsubscribe request");
		return ERR_INVALID_PARAMETER;
	}

	if (__removeListener(REQ_SUBSCRIBE, rid, listener) <= 0) {
		__unsubscribe(subject.c_str(), rid);
	}
	_D(YELLOW("Unsubscribe event(rule%d). req%d"), ruleId, rid);

	return ERR_NONE;
}

void ContextMonitor::__unsubscribe(const char *subject, int subscriptionId)
{
	FactRequest *req = new(std::nothrow) FactRequest(REQ_UNSUBSCRIBE, subscriptionId, subject, NULL, NULL);
	IF_FAIL_VOID_TAG(req, _E, "Memory allocation failed");

	__contextMgr->assignRequest(req);
	__removeSub(REQ_SUBSCRIBE, subscriptionId);
}

int ContextMonitor::read(std::string subject, Json option, IContextListener* listener)
{
	int reqId = __read(subject.c_str(), &option, listener);
	IF_FAIL_RETURN_TAG(reqId > 0, ERR_OPERATION_FAILED, _E, "Read condition failed");
	_D(YELLOW("Read condition(%s). req%d"), subject.c_str(), reqId);

	return ERR_NONE;
}

int ContextMonitor::__read(const char* subject, Json* option, IContextListener* listener)
{
	IF_FAIL_RETURN(subject, ERR_INVALID_PARAMETER);

	int rid = __findSub(REQ_READ, subject, option);
	if (rid > 0) {
		__addListener(REQ_READ, rid, listener);
		_D("Duplicated request for %s", subject);
		return rid;
	}

	rid = __generateReqId();

	FactRequest *req = new(std::nothrow) FactRequest(REQ_READ,
			rid, subject, option ? option->str().c_str() : NULL, this);
	IF_FAIL_RETURN_TAG(req, -1, _E, "Memory allocation failed");

	__contextMgr->assignRequest(req);
	__addSub(REQ_READ, rid, subject, option, listener);

	if (__lastErr != ERR_NONE) {
		_E("Read request failed: %#x", __lastErr);
		return -1;
	}

	return rid;
}

bool ContextMonitor::isSupported(std::string subject)
{
	return __contextMgr->isSupported(subject.c_str());
}

bool ContextMonitor::isAllowed(const char *client, const char *subject)
{
	//TODO: re-implement this in the proper 3.0 style
	//return __contextMgr->isAllowed(client, subject);
	return true;
}

int ContextMonitor::__findSub(RequestType type, const char* subject, Json* option)
{
	// @return	request id
	std::map<int, SubscrInfo*>* map = (type == REQ_SUBSCRIBE)? &__subscrMap : &___readMap;

	Json jOpt;
	if (option) {
		jOpt = *option;
	}

	for (auto it = map->begin(); it != map->end(); ++it) {
		if ((*(it->second)).subject == subject && (*(it->second)).option == jOpt) {
			return it->first;
		}
	}

	return -1;
}

bool ContextMonitor::__addSub(RequestType type, int sid, const char* subject, Json* option, IContextListener* listener)
{
	std::map<int, SubscrInfo*>* map = (type == REQ_SUBSCRIBE)? &__subscrMap : &___readMap;

	SubscrInfo *info = new(std::nothrow) SubscrInfo(sid, subject, option);
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");
	info->listenerList.push_back(listener);

	map->insert(std::pair<int, SubscrInfo*>(sid, info));
	return true;
}

void ContextMonitor::__removeSub(RequestType type, const char* subject, Json* option)
{
	std::map<int, SubscrInfo*>* map = (type == REQ_SUBSCRIBE)? &__subscrMap : &___readMap;

	Json jOpt;
	if (option) {
		jOpt = *option;
	}

	for (auto it = map->begin(); it != map->end(); ++it) {
		if ((*(it->second)).subject == subject && (*(it->second)).option == jOpt) {
			delete it->second;
			map->erase(it);
			return;
		}
	}
}

void ContextMonitor::__removeSub(RequestType type, int sid)
{
	std::map<int, SubscrInfo*>* map = (type == REQ_SUBSCRIBE)? &__subscrMap : &___readMap;

	SubscrInfo* info = map->at(sid);
	info->listenerList.clear();

	delete info;
	map->erase(sid);

	return;
}

int ContextMonitor::__addListener(RequestType type, int sid, IContextListener* listener)
{
	// @return	number of listeners for the corresponding sid
	std::map<int, SubscrInfo*>* map = (type == REQ_SUBSCRIBE)? &__subscrMap : &___readMap;

	auto it = map->find(sid);

	SubscrInfo* info = it->second;
	info->listenerList.push_back(listener);

	return info->listenerList.size();
}

int ContextMonitor::__removeListener(RequestType type, int sid, IContextListener* listener)
{
	// @return	number of listeners for the corresponding sid
	std::map<int, SubscrInfo*>* map = (type == REQ_SUBSCRIBE)? &__subscrMap : &___readMap;

	auto it = map->find(sid);

	SubscrInfo* info = it->second;

	for (auto it2 = info->listenerList.begin(); it2 != info->listenerList.end(); ++it2) {
		if (*it2 == listener) {
			info->listenerList.erase(it2);
			break;
		}
	}

	return info->listenerList.size();
}

void ContextMonitor::replyResult(int reqId, int error, Json* requestResult)
{
	_D("Request result received: %d", reqId);

	__lastRid = reqId;
	__lastErr = error;
}

void ContextMonitor::replyResult(int reqId, int error, const char* subject, Json* option, Json* fact)
{
	_D(YELLOW("Condition received: subject(%s), option(%s), fact(%s)"), subject, option->str().c_str(), fact->str().c_str());

	auto it = ___readMap.find(reqId);
	IF_FAIL_VOID_TAG(it != ___readMap.end(), _E, "Request id not found");

	SubscrInfo* info = it->second;
	for (auto it2 = info->listenerList.begin(); it2 != info->listenerList.end(); ++it2) {
		(*it2)->onConditionReceived(subject, *option, *fact);
	}

	__removeSub(REQ_READ, reqId);
}

void ContextMonitor::publishFact(int reqId, int error, const char* subject, Json* option, Json* fact)
{
	_D(YELLOW("Event received: subject(%s), option(%s), fact(%s)"), subject, option->str().c_str(), fact->str().c_str());

	auto it = __subscrMap.find(reqId);
	IF_FAIL_VOID_TAG(it != __subscrMap.end(), _E, "Request id not found");

	SubscrInfo* info = it->second;
	for (auto it2 = info->listenerList.begin(); it2 != info->listenerList.end(); ++it2) {
		(*it2)->onEventReceived(subject, *option, *fact);
	}
}
