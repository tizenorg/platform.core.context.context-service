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
#include "access_control/privilege.h"
#include "server.h"
#include "request.h"
#include "provider.h"

ctx::context_provider_handler::context_provider_handler(const char *subj, ctx::context_provider_info &prvd) :
	subject(subj),
	provider_info(prvd)
{
}

ctx::context_provider_handler::~context_provider_handler()
{
	for (auto it = subscribe_requests.begin(); it != subscribe_requests.end(); ++it) {
		delete *it;
	}
	subscribe_requests.clear();

	for (auto it = read_requests.begin(); it != read_requests.end(); ++it) {
		delete *it;
	}
	read_requests.clear();

	provider_info.destroy(provider_info.data);
}

bool ctx::context_provider_handler::is_allowed(const ctx::credentials *creds)
{
	IF_FAIL_RETURN(creds, true);	/* In case of internal requests */
	return privilege_manager::is_allowed(creds, provider_info.privilege);
}

ctx::context_provider_iface* ctx::context_provider_handler::get_provider(ctx::request_info *request)
{
	context_provider_iface *provider = provider_info.create(provider_info.data);
	if (!provider) {
		_E("Memory allocation failed");
		delete request;
		return NULL;
	}

	return provider;
}

void ctx::context_provider_handler::subscribe(ctx::request_info *request)
{
	_I(CYAN("'%s' subscribes '%s' (RID-%d)"), request->get_client(), subject, request->get_id());

	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	ctx::Json request_result;
	int error = provider->subscribe(subject, request->get_description().str(), &request_result);

	if (!request->reply(error, request_result) || error != ERR_NONE) {
		delete request;
		return;
	}

	subscribe_requests.push_back(request);
}

void ctx::context_provider_handler::unsubscribe(ctx::request_info *request)
{
	_I(CYAN("'%s' unsubscribes '%s' (RID-%d)"), request->get_client(), subject, request->get_id());

	// Search the subscribe request to be removed
	auto target = find_request(subscribe_requests, request->get_client(), request->get_id());
	if (target == subscribe_requests.end()) {
		_W("Unknown request");
		delete request;
		return;
	}

	// Keep the pointer to the request found
	request_info *req_found = *target;

	// Remove the request from the list
	subscribe_requests.erase(target);

	// Check if there exist the same requests
	if (find_request(subscribe_requests, req_found->get_description()) != subscribe_requests.end()) {
		// Do not stop detecting the subject
		_D("A same request from '%s' exists", req_found->get_client());
		request->reply(ERR_NONE);
		delete request;
		delete req_found;
		return;
	}

	// Get the provider
	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	// Stop detecting the subject
	int error = provider->unsubscribe(subject, req_found->get_description());
	request->reply(error);
	delete request;
	delete req_found;
}

void ctx::context_provider_handler::read(ctx::request_info *request)
{
	_I(CYAN("'%s' reads '%s' (RID-%d)"), request->get_client(), subject, request->get_id());

	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	ctx::Json request_result;
	int error = provider->read(subject, request->get_description().str(), &request_result);

	if (!request->reply(error, request_result) || error != ERR_NONE) {
		delete request;
		return;
	}

	read_requests.push_back(request);
}

void ctx::context_provider_handler::write(ctx::request_info *request)
{
	_I(CYAN("'%s' writes '%s' (RID-%d)"), request->get_client(), subject, request->get_id());

	context_provider_iface *provider = get_provider(request);
	IF_FAIL_VOID(provider);

	ctx::Json request_result;
	int error = provider->write(subject, request->get_description(), &request_result);

	request->reply(error, request_result);
	delete request;
}

bool ctx::context_provider_handler::publish(ctx::Json &option, int error, ctx::Json &data_updated)
{
	auto end = subscribe_requests.end();
	auto target = find_request(subscribe_requests.begin(), end, option);

	while (target != end) {
		if (!(*target)->publish(error, data_updated)) {
			return false;
		}
		target = find_request(++target, end, option);
	}

	return true;
}

bool ctx::context_provider_handler::reply_to_read(ctx::Json &option, int error, ctx::Json &data_read)
{
	auto end = read_requests.end();
	auto target = find_request(read_requests.begin(), end, option);
	auto prev = target;

	ctx::Json dummy;

	while (target != end) {
		(*target)->reply(error, dummy, data_read);
		prev = target;
		target = find_request(++target, end, option);

		delete *prev;
		read_requests.erase(prev);
	}

	return true;
}

ctx::context_provider_handler::request_list_t::iterator
ctx::context_provider_handler::find_request(request_list_t &r_list, Json &option)
{
	return find_request(r_list.begin(), r_list.end(), option);
}

ctx::context_provider_handler::request_list_t::iterator
ctx::context_provider_handler::find_request(request_list_t &r_list, std::string client, int req_id)
{
	for (auto it = r_list.begin(); it != r_list.end(); ++it) {
		if (client == (*it)->get_client() && req_id == (*it)->get_id()) {
			return it;
		}
	}
	return r_list.end();
}

ctx::context_provider_handler::request_list_t::iterator
ctx::context_provider_handler::find_request(request_list_t::iterator begin, request_list_t::iterator end, Json &option)
{
	for (auto it = begin; it != end; ++it) {
		if (option == (*it)->get_description()) {
			return it;
		}
	}
	return end;
}
