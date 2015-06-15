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

#ifndef __CONTEXT_TIMER_MANAGER_IMPL_H__
#define __CONTEXT_TIMER_MANAGER_IMPL_H__

#include <timer_listener_iface.h>
#include <timer_mgr_iface.h>

namespace ctx {

	class timer_manager_impl : public timer_manager_iface {
		private:
			bool initialized;

		public:
			timer_manager_impl();
			~timer_manager_impl();

			bool init();
			void release();

			int set_for(int interval, timer_listener_iface* listener, void* user_data);
			int set_at(int hour, int min, int day_of_week, timer_listener_iface* listener, void* user_data);
			void remove(int timer_id);

	};	/* class timer_manager */
}

#endif	/* __CONTEXT_TIMER_MANAGER_IMPL_H__ */
