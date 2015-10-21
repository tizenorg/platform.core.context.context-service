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

	class json;
	class credentials;
	class request_info;

	class context_provider_handler {
	public:
		typedef std::list<request_info*> request_list_t;

		context_provider_handler(const char *subj, context_provider_info &prvd);
		~context_provider_handler();

		bool is_allowed(const credentials *creds);

		void subscribe(request_info *request);
		void unsubscribe(request_info *request);
		void read(request_info *request);
		void write(request_info *request);

		bool publish(ctx::json &option, int error, ctx::json &data_updated);
		bool reply_to_read(ctx::json &option, int error, ctx::json &data_read);

	private:
		const char *subject;
		context_provider_info provider_info;
		request_list_t subscribe_requests;
		request_list_t read_requests;

		context_provider_iface* get_provider(request_info *request);
		request_list_t::iterator find_request(request_list_t &r_list, json &option);
		request_list_t::iterator find_request(request_list_t &r_list, std::string client, int req_id);
		request_list_t::iterator find_request(request_list_t::iterator begin, request_list_t::iterator end, json &option);

	};	/* class context_provider_handler */

}	/* namespace ctx */

#endif	/* __CONTEXT_PROVIDER_HANDLER_H__ */
