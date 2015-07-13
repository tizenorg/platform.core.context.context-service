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

#ifndef __CONTEXT_TRIGGER_FACT_REQUEST_H__
#define __CONTEXT_TRIGGER_FACT_REQUEST_H__

#include "fact_reader.h"
#include "../request.h"

namespace ctx {

	class fact_request : public request_info {
		public:
			fact_request(int type, const char* client, int req_id, const char* subj, const char* desc, const char* zone, fact_reader* reader);
			~fact_request();

			void set_zone_name(const char* zone_name);
			bool reply(int error);
			bool reply(int error, ctx::json& request_result);
			bool reply(int error, ctx::json& request_result, ctx::json& data_read);
			bool publish(int error, ctx::json& data);

		private:
			fact_reader *_reader;
			bool replied;
	};

}	/* namespace ctx */

#endif	/* End of __CONTEXT_TRIGGER_FACT_REQUEST_H__ */
