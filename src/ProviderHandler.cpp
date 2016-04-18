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
#include <Types.h>
#include "access_control/Privilege.h"
#include "Request.h"
#include "ProviderHandler.h"

using namespace ctx;

ProviderHandler::ProviderHandler(const char *subject, const char *privilege, ContextProvider *provider) :
	__subject(subject),
	__privilege(privilege),
	__provider(provider)
{
}

ProviderHandler::~ProviderHandler()
{
	for (RequestInfo*& info : __subscribeRequests) {
		delete info;
	}
	__subscribeRequests.clear();

	for (RequestInfo*& info : __readRequests) {
		delete info;
	}
	__readRequests.clear();

	delete __provider;
}

bool ProviderHandler::isAllowed(const Credentials *creds)
{
	IF_FAIL_RETURN(creds, true);	/* In case of internal requests */
	return privilege_manager::isAllowed(creds, __privilege);
}

ContextProvider* ProviderHandler::__getProvider(RequestInfo *request)
{
	/* TODO: When implementing dynamic so loading... */
	return __provider;
}

void ProviderHandler::subscribe(RequestInfo *request)
{
	_I(CYAN("'%s' subscribes '%s' (RID-%d)"), request->getClient(), __subject, request->getId());

	ContextProvider *provider = __getProvider(request);
	IF_FAIL_VOID(provider);

	Json requestResult;
	int error = provider->subscribe(request->getDescription().str(), &requestResult);

	if (!request->reply(error, requestResult) || error != ERR_NONE) {
		delete request;
		return;
	}

	__subscribeRequests.push_back(request);
}

void ProviderHandler::unsubscribe(RequestInfo *request)
{
	_I(CYAN("'%s' unsubscribes '%s' (RID-%d)"), request->getClient(), __subject, request->getId());

	/* Search the subscribe request to be removed */
	auto target = __findRequest(__subscribeRequests, request->getClient(), request->getId());
	if (target == __subscribeRequests.end()) {
		_W("Unknown request");
		delete request;
		return;
	}

	/* Keep the pointer to the request found */
	RequestInfo *reqFound = *target;

	/* Remove the request from the list */
	__subscribeRequests.erase(target);

	/* Check if there exist the same requests */
	if (__findRequest(__subscribeRequests, reqFound->getDescription()) != __subscribeRequests.end()) {
		/* Do not stop detecting the subject */
		_D("A same request from '%s' exists", reqFound->getClient());
		request->reply(ERR_NONE);
		delete request;
		delete reqFound;
		return;
	}

	/* Get the provider */
	ContextProvider *provider = __getProvider(request);
	IF_FAIL_VOID(provider);

	/* Stop detecting the subject */
	int error = provider->unsubscribe(reqFound->getDescription());
	request->reply(error);
	delete request;
	delete reqFound;
}

void ProviderHandler::read(RequestInfo *request)
{
	_I(CYAN("'%s' reads '%s' (RID-%d)"), request->getClient(), __subject, request->getId());

	ContextProvider *provider = __getProvider(request);
	IF_FAIL_VOID(provider);

	Json requestResult;
	int error = provider->read(request->getDescription().str(), &requestResult);

	if (!request->reply(error, requestResult) || error != ERR_NONE) {
		delete request;
		return;
	}

	__readRequests.push_back(request);
}

void ProviderHandler::write(RequestInfo *request)
{
	_I(CYAN("'%s' writes '%s' (RID-%d)"), request->getClient(), __subject, request->getId());

	ContextProvider *provider = __getProvider(request);
	IF_FAIL_VOID(provider);

	Json requestResult;
	int error = provider->write(request->getDescription(), &requestResult);

	request->reply(error, requestResult);
	delete request;
}

bool ProviderHandler::publish(Json &option, int error, Json &dataUpdated)
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

bool ProviderHandler::replyToRead(Json &option, int error, Json &dataRead)
{
	auto end = __readRequests.end();
	auto target = __findRequest(__readRequests.begin(), end, option);
	auto prev = target;

	Json dummy;

	while (target != end) {
		(*target)->reply(error, dummy, dataRead);
		prev = target;
		target = __findRequest(++target, end, option);

		delete *prev;
		__readRequests.erase(prev);
	}

	return true;
}

ProviderHandler::RequestList::iterator
ProviderHandler::__findRequest(RequestList &reqList, Json &option)
{
	return __findRequest(reqList.begin(), reqList.end(), option);
}

ProviderHandler::RequestList::iterator
ProviderHandler::__findRequest(RequestList &reqList, std::string client, int reqId)
{
	for (auto it = reqList.begin(); it != reqList.end(); ++it) {
		if (client == (*it)->getClient() && reqId == (*it)->getId()) {
			return it;
		}
	}
	return reqList.end();
}

ProviderHandler::RequestList::iterator
ProviderHandler::__findRequest(RequestList::iterator begin, RequestList::iterator end, Json &option)
{
	for (auto it = begin; it != end; ++it) {
		if (option == (*it)->getDescription()) {
			return it;
		}
	}
	return end;
}
