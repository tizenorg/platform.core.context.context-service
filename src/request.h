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

#ifndef __CONTEXT_REQUEST_INFO_H__
#define __CONTEXT_REQUEST_INFO_H__

#include <string>
#include <json.h>

namespace ctx {

	/* Forward declaration */
	class credentials;

	class request_info {
	public:
		request_info(int type, int req_id, const char *subj, const char *desc);
		virtual ~request_info();

		int get_type();
		int get_id();
		const char* get_subject();
		ctx::json& get_description();

		virtual const credentials* get_credentials();
		virtual const char* get_package_id();
		/* TODO: remove this get_client() */
		virtual const char* get_client();
		virtual bool reply(int error) = 0;
		virtual bool reply(int error, ctx::json &request_result) = 0;
		virtual bool reply(int error, ctx::json &request_result, ctx::json &data_read) = 0;
		virtual bool publish(int error, ctx::json &data) = 0;

	protected:
		int _type;
		int _req_id;
		std::string _subject;
		ctx::json _description;
	};

}	/* namespace ctx */

#endif	/* End of __CONTEXT_REQUEST_INFO_H__ */
