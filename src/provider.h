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

#ifndef __CONTEXT_PROVIDER_HANDLER_H__
#define __CONTEXT_PROVIDER_HANDLER_H__

#include <string>
#include <list>
#include <provider_iface.h>

namespace ctx {

	class Json;
	class credentials;
	class RequestInfo;

	class context_provider_handler {
	public:
		typedef std::list<RequestInfo*> request_list_t;

		context_provider_handler(const char *subj, context_provider_info &prvd);
		~context_provider_handler();

		bool is_allowed(const credentials *creds);

		void subscribe(RequestInfo *request);
		void unsubscribe(RequestInfo *request);
		void read(RequestInfo *request);
		void write(RequestInfo *request);

		bool publish(ctx::Json &option, int error, ctx::Json &data_updated);
		bool reply_to_read(ctx::Json &option, int error, ctx::Json &data_read);

	private:
		const char *subject;
		context_provider_info provider_info;
		request_list_t subscribe_requests;
		request_list_t read_requests;

		context_provider_iface* get_provider(RequestInfo *request);
		request_list_t::iterator find_request(request_list_t &r_list, Json &option);
		request_list_t::iterator find_request(request_list_t &r_list, std::string client, int req_id);
		request_list_t::iterator find_request(request_list_t::iterator begin, request_list_t::iterator end, Json &option);

	};	/* class context_provider_handler */

}	/* namespace ctx */

#endif	/* __CONTEXT_PROVIDER_HANDLER_H__ */
