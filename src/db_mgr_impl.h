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

#ifndef __CONTEXT_DB_MANAGER_IMPL_H__
#define __CONTEXT_DB_MANAGER_IMPL_H__

#include <vector>
#include <json.h>
#include <sqlite3.h>

#include <event_driven.h>
#include <db_listener_iface.h>
#include <db_mgr_iface.h>

namespace ctx {

	class db_manager_impl : public event_driven_thread, public db_manager_iface {
	private:
		enum query_type_e {
			QTYPE_CREATE_TABLE = 1,
			QTYPE_INSERT,
			QTYPE_EXECUTE,
		};

		struct query_info_s {
			unsigned int id;
			db_listener_iface* listener;
			std::string query;
		};

		struct query_result_s {
			int type;
			unsigned int id;
			int error;
			db_listener_iface* listener;
			std::vector<json>* result;
		};

		sqlite3 *db_handle;

		void on_thread_event_popped(int type, void* data);
		void delete_thread_event(int type, void* data);

		bool open();
		void close();

		std::string compose_create_query(const char* table_name, const char* columns, const char* option);
		std::string compose_insert_query(const char* table_name, json& record);

		void _execute(int query_type, unsigned int query_id, const char* query, db_listener_iface* listener);
		void send_result(int query_type, unsigned int query_id, db_listener_iface* listener, int error, std::vector<json>* result);

		static int execution_result_cb(void *user_data, int dim, char **value, char **column);
		static gboolean _send_result(gpointer data);

	public:
		db_manager_impl();
		~db_manager_impl();

		bool init();
		void release();

		bool create_table(unsigned int query_id, const char* table_name, const char* columns, const char* option = NULL, db_listener_iface* listener = NULL);
		bool insert(unsigned int query_id, const char* table_name, json& record, db_listener_iface* listener = NULL);
		bool execute(unsigned int query_id, const char* query, db_listener_iface* listener);

		bool create_table_sync(const char* table_name, const char* columns, const char* option = NULL);
		bool insert_sync(const char* table_name, json& record, int64_t* row_id);
		bool execute_sync(const char* query, std::vector<json>* records);

	};	/* class db_manager */
}

#endif	/* __CONTEXT_DB_MANAGER_IMPL_H__ */
