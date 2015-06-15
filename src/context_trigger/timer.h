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

#ifndef __CONTEXT_CONTEXT_TRIGGER_TIMER_H__
#define __CONTEXT_CONTEXT_TRIGGER_TIMER_H__

#include <timer_mgr.h>
#include <timer_listener_iface.h>
#include <string>
#include <map>

namespace ctx {

	class context_trigger;

	class trigger_timer : public timer_listener_iface {
	private:
		struct ref_count_array_s {
			int count[7];	/* reference counts for days of week*/
			ref_count_array_s();
		};

		struct timer_state_s {
			int timer_id;
			int day_of_week; /* day of week, merged into one integer */
			timer_state_s() : timer_id(-1), day_of_week(0) {}
		};

		typedef std::map<int, ref_count_array_s> ref_count_map_t;
		typedef std::map<int, timer_state_s> timer_state_map_t;

		ctx::context_trigger *trigger;
		std::string zone;
		ref_count_map_t ref_count_map;
		timer_state_map_t timer_state_map;

		int merge_day_of_week(int *ref_cnt);
		bool reset_timer(int hour);
		void on_timer_expired(int hour, int min, int day_of_week);

	protected:
		bool on_timer_expired(int timer_id, void *user_data);

	public:
		trigger_timer(ctx::context_trigger *tr, std::string z);
		~trigger_timer();

		bool add(int minute, int day_of_week);
		bool remove(int minute, int day_of_week);
		void clear();
		static int get_day_of_month();
		static std::string get_day_of_week();
		static int get_minute_of_day();
		static int convert_string_to_day_of_week(std::string d);
		static std::string convert_day_of_week_to_string(int d);
		bool empty();
	};

}	/* namespace ctx */

#endif	/* End of __CONTEXT_CONTEXT_TRIGGER_TIMER_H__ */
