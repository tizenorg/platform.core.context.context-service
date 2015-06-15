/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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

#ifndef __CONTEXT_VASUM_ZONE_UTIL_IMPL_H__
#define __CONTEXT_VASUM_ZONE_UTIL_IMPL_H__

#include <sys/types.h>
#include <zone_util.h>
#include <zone_util_iface.h>

namespace ctx {

	class zone_util_impl : public zone_util_iface {
		public:
			zone_util_impl() {}
			~zone_util_impl() {}
			void* join_by_name(const char* name);
			void* join_to_zone(void* zone);
			const char* default_zone();
	};

	namespace zone_util {
		bool init();
		void release();
		const char* get_name_by_pid(pid_t pid);
	}

}	/* namespace ctx */

#endif	/* End of __CONTEXT_VASUM_ZONE_UTIL_IMPL_H__ */
