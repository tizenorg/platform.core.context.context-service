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

#include <glib.h>
#include <types_internal.h>
#include <Json.h>
#include "access_control/Privilege.h"
#include "Server.h"
#include "Request.h"
#include "ProviderHandler.h"

ctx::ProviderHandler::ProviderHandler(const char *subj, ctx::ContextProviderInfo &prvd) :
	__subject(subj),
	__providerInfo(prvd)
{
}

ctx::ProviderHandler::~ProviderHandler()
{
	for (auto it = __subscribeRequests.begin(); it != __subscribeRequests.end(); ++it) {
		delete *it;
	}
	__subscribeRequests.clear();

	for (auto it = __readRequests.begin(); it != __readRequests.end(); ++it) {
		delete *it;
	}
	__readRequests.clear();

	__providerInfo.destroy(__providerInfo.data);
}

bool ctx::ProviderHandler::isAllowed(const ctx::Credentials *creds)
{
	IF_FAIL_RETURN(creds, true);	/* In case of internal requests */
	return privilege_manager::isAllowed(creds, __providerInfo.privilege);
}

ctx::ContextProviderBase* ctx::ProviderHandler::__getProvider(ctx::RequestInfo *request)
{
	ContextProviderBase *provider = __providerInfo.create(__providerInfo.data);
	if (!provider) {
		_E("Memory allocation failed");
		delete request;
		return NULL;
	}

	return provider;
}

void ctx::ProviderHandler::subscribe(ctx::RequestInfo *request)
{
	_I(CYAN("'%s' subscribes '%s' (RID-%d)"), request->getClient(), __subject, request->getId());

	ContextProviderBase *provider = __getProvider(request);
	IF_FAIL_VOID(provider);

	ctx::Json requestResult;
	int error = provider->subscribe(__subject, request->getDescription().str(), &requestResult);

	if (!request->reply(error, requestResult) || error != ERR_NONE) {
		delete request;
		return;
	}

	__subscribeRequests.push_back(request);
}

void ctx::ProviderHandler::unsubscribe(ctx::RequestInfo *request)
{
	_I(CYAN("'%s' unsubscribes '%s' (RID-%d)"), request->getClient(), __subject, request->getId());

	// Search the subscribe request to be removed
	auto target = __findRequest(__subscribeRequests, request->getClient(), request->getId());
	if (target == __subscribeRequests.end()) {
		_W("Unknown request");
		delete request;
		return;
	}

	// Keep the pointer to the request found
	RequestInfo *req_found = *target;

	// Remove the request from the list
	__subscribeRequests.erase(target);

	// Check if there exist the same requests
	if (__findRequest(__subscribeRequests, req_found->getDescription()) != __subscribeRequests.end()) {
		// Do not stop detecting the subject
		_D("A same request from '%s' exists", req_found->getClient());
		request->reply(ERR_NONE);
		delete request;
		delete req_found;
		return;
	}

	// Get the provider
	ContextProviderBase *provider = __getProvider(request);
	IF_FAIL_VOID(provider);

	// Stop detecting the subject
	int error = provider->unsubscribe(__subject, req_found->getDescription());
	request->reply(error);
	delete request;
	delete req_found;
}

void ctx::ProviderHandler::read(ctx::RequestInfo *request)
{
	_I(CYAN("'%s' reads '%s' (RID-%d)"), request->getClient(), __subject, request->getId());

	ContextProviderBase *provider = __getProvider(request);
	IF_FAIL_VOID(provider);

	ctx::Json requestResult;
	int error = provider->read(__subject, request->getDescription().str(), &requestResult);

	if (!request->reply(error, requestResult) || error != ERR_NONE) {
		delete request;
		return;
	}

	__readRequests.push_back(request);
}

void ctx::ProviderHandler::write(ctx::RequestInfo *request)
{
	_I(CYAN("'%s' writes '%s' (RID-%d)"), request->getClient(), __subject, request->getId());

	ContextProviderBase *provider = __getProvider(request);
	IF_FAIL_VOID(provider);

	ctx::Json requestResult;
	int error = provider->write(__subject, request->getDescription(), &requestResult);

	request->reply(error, requestResult);
	delete request;
}

bool ctx::ProviderHandler::publish(ctx::Json &option, int error, ctx::Json &dataUpdated)
{
	auto end = __subscribeRequests.end();
	auto target = __findRequest(__subscribeRequests.begin(), end, option);

	while (target != end) {
		if (!(*target)->publish(error, dataUpdated)) {
			return false;
		}
		target = __findRequest(++target, end, option);
	}

	return true;
}

bool ctx::ProviderHandler::replyToRead(ctx::Json &option, int error, ctx::Json &dataRead)
{
	auto end = __readRequests.end();
	auto target = __findRequest(__readRequests.begin(), end, option);
	auto prev = target;

	ctx::Json dummy;

	while (target != end) {
		(*target)->reply(error, dummy, dataRead);
		prev = target;
		target = __findRequest(++target, end, option);

		delete *prev;
		__readRequests.erase(prev);
	}

	return true;
}

ctx::ProviderHandler::RequestList::iterator
ctx::ProviderHandler::__findRequest(RequestList &reqList, Json &option)
{
	return __findRequest(reqList.begin(), reqList.end(), option);
}

ctx::ProviderHandler::RequestList::iterator
ctx::ProviderHandler::__findRequest(RequestList &reqList, std::string client, int reqId)
{
	for (auto it = reqList.begin(); it != reqList.end(); ++it) {
		if (client == (*it)->getClient() && reqId == (*it)->getId()) {
			return it;
		}
	}
	return reqList.end();
}

ctx::ProviderHandler::RequestList::iterator
ctx::ProviderHandler::__findRequest(RequestList::iterator begin, RequestList::iterator end, Json &option)
{
	for (auto it = begin; it != end; ++it) {
		if (option == (*it)->getDescription()) {
			return it;
		}
	}
	return end;
}
