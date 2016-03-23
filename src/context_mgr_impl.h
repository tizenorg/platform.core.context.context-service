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

#ifndef __CONTEXT_MANAGER_IMPL_H__
#define __CONTEXT_MANAGER_IMPL_H__

#include <string>
#include <map>
#include <glib.h>
#include <context_mgr.h>
#include <context_mgr_iface.h>

namespace ctx {

	/* Forward declaration */
	class credentials;
	class request_info;
	class context_provider_handler;

	class context_manager_impl : public context_manager_iface {
	public:
		context_manager_impl();
		~context_manager_impl();

		bool init();
		void release();

		void assign_request(ctx::request_info *request);
		bool is_supported(const char *subject);
		bool is_allowed(const credentials *creds, const char *subject);
		bool pop_trigger_item(std::string &subject, int &operation, ctx::Json &attributes, ctx::Json &options, std::string &owner, bool& unregister);

		/* From the interface class */
		bool register_provider(const char *subject, context_provider_info &provider_info);
		bool unregister_provider(const char *subject);
		bool register_trigger_item(const char *subject, int operation, ctx::Json attributes, ctx::Json options, const char *owner = NULL);
		bool unregister_trigger_item(const char *subject);
		bool publish(const char *subject, ctx::Json &option, int error, ctx::Json &data_updated);
		bool reply_to_read(const char *subject, ctx::Json &option, int error, ctx::Json &data_read);

	private:
		std::map<std::string, context_provider_handler*> provider_handle_map;
		static bool initialized;

		static gboolean thread_switcher(gpointer data);
		void _publish(const char *subject, ctx::Json &option, int error, ctx::Json &data_updated);
		void _reply_to_read(const char *subject, ctx::Json &option, int error, ctx::Json &data_read);

		/* For custom request */
		bool handle_custom_request(ctx::request_info* request);
	};	/* class context_manager_impl */

}	/* namespace ctx */

#endif	/* End of __CONTEXT_MANAGER_IMPL_H__ */
