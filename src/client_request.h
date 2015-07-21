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

#ifndef __CONTEXT_CLIENT_REQUEST_H__
#define __CONTEXT_CLIENT_REQUEST_H__

#include <gio/gio.h>
#include "request.h"

namespace ctx {

	class client_request : public request_info {
		public:
			client_request(int type, const char* client, int req_id, const char* subj, const char* desc, GDBusMethodInvocation *inv);
			~client_request();

			bool set_peer_creds(const char *smack_label);
			const char* get_app_id();

			bool reply(int error);
			bool reply(int error, ctx::json& request_result);
			bool reply(int error, ctx::json& request_result, ctx::json& data_read);
			bool publish(int error, ctx::json& data);

		private:
			GDBusMethodInvocation *invocation;
			std::string client_app_id;
	};

}	/* namespace ctx */

#endif	/* End of __CONTEXT_CLIENT_REQUEST_H__ */
