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

#ifndef __CONTEXT_ACTION_MANAGER_H__
#define __CONTEXT_ACTION_MANAGER_H__

namespace ctx {

	class json;

	namespace action_manager {

		void trigger_action(ctx::json& action, std::string pkg_id);

	}

}	/* namespace ctx */

#endif	/* End of __CONTEXT_ACTION_MANAGER_H__ */
