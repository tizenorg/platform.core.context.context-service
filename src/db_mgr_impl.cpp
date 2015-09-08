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

#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <list>
#include <tzplatform_config.h>
#include <scope_mutex.h>
#include "server.h"
#include "db_mgr_impl.h"

#define CONTEXT_DB_PATH tzplatform_mkpath(TZ_USER_DB, ".context-service.db")

static bool initialized = false;
static GThread *mainthread = NULL;
static GMutex sync_insert_mutex;
static GMutex sync_execute_mutex;
static GCond sync_insert_cond;
static GCond sync_execute_cond;
static int64_t sync_insert_row_id = -1;
static unsigned int sync_insert_qid = 0;
static int sync_execute_error;
static std::vector<ctx::json> sync_execute_result;
static unsigned int sync_execute_qid = 0;

ctx::db_manager_impl::db_manager_impl()
	: db_handle(NULL)
{
}

ctx::db_manager_impl::~db_manager_impl()
{
	release();
}

bool ctx::db_manager_impl::init()
{
	IF_FAIL_RETURN_TAG(!initialized, true, _W, "Re-initialization");
	IF_FAIL_RETURN(open(), false);
	mainthread = g_thread_self();
	IF_FAIL_RETURN(start(), false);
	initialized = true;
	return true;
}

void ctx::db_manager_impl::release()
{
	IF_FAIL_VOID(initialized);
	initialized = false;
	stop();
	close();
}

bool ctx::db_manager_impl::open()
{
	sqlite3 *db = NULL;
	int r = sqlite3_open(CONTEXT_DB_PATH, &db);

	IF_FAIL_RETURN_TAG(r == SQLITE_OK, false, _E, "Path: %s / Error: %s", CONTEXT_DB_PATH, sqlite3_errmsg(db));

	db_handle = db;
	return true;
}

void ctx::db_manager_impl::close()
{
	if (db_handle) {
		sqlite3_close(db_handle);
	}
	db_handle = NULL;
}

bool ctx::db_manager_impl::is_main_thread()
{
	return mainthread == g_thread_self();
}

void ctx::db_manager_impl::on_thread_event_popped(int type, void* data)
{
	IF_FAIL_VOID(data);
	query_info_s *info = static_cast<query_info_s*>(data);

	switch (type) {
		case QTYPE_CREATE_TABLE:
		case QTYPE_INSERT:
		case QTYPE_EXECUTE:
			_execute(type, info->id, info->query.c_str(), info->listener);
			break;
		default:
			_W("Unknown type: %d", type);
			break;
	}

	delete_thread_event(type, data);
}

void ctx::db_manager_impl::delete_thread_event(int type, void* data)
{
	IF_FAIL_VOID(data);
	query_info_s *info = static_cast<query_info_s*>(data);
	delete info;
}

bool ctx::db_manager_impl::create_table(unsigned int query_id, const char* table_name, const char* columns, const char* option, db_listener_iface* listener)
{
	IF_FAIL_RETURN_TAG(initialized, false, _E, "Not initialized");

	query_info_s *info = new(std::nothrow) query_info_s;
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");

	info->query = "CREATE TABLE IF NOT EXISTS ";
	info->query = info->query + table_name + " (row_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT," + columns + ")";
	if (option) {
		info->query = info->query + " " + option;
	}
	info->query += ";";

	info->id = query_id;
	info->listener = listener;

	if (!push_thread_event(QTYPE_CREATE_TABLE, info)) {
		_E("Pushing thread event failed");
		delete info;
		return false;
	}

	return true;
}

bool ctx::db_manager_impl::insert(unsigned int query_id, const char* table_name, json& record, db_listener_iface* listener)
{
	IF_FAIL_RETURN_TAG(initialized, false, _E, "Not initialized");

	std::list<std::string> keys;
	IF_FAIL_RETURN_TAG(record.get_keys(&keys), false, _E, "Invalid record");

	std::ostringstream colstream;
	std::ostringstream valstream;

	for (std::list<std::string>::iterator it = keys.begin(); it != keys.end(); ++it) {
		std::string s;
		int64_t i;
		if (record.get(NULL, (*it).c_str(), &s)) {
			colstream << *it << ",";

			char* buf = sqlite3_mprintf("%Q", s.c_str());
			IF_FAIL_RETURN_TAG(buf, false, _E, "Memory allocation failed");
			valstream << buf << ",";
			sqlite3_free(buf);
		} else if (record.get(NULL, (*it).c_str(), &i)) {
			colstream << *it << ",";
			valstream << i << ",";
		}
	}

	std::string cols = colstream.str();
	std::string vals = valstream.str();

	IF_FAIL_RETURN_TAG(!cols.empty(), false, _E, "Invalid record");

	cols.erase(cols.size() - 1);
	vals.erase(vals.size() - 1);

	query_info_s *info = new(std::nothrow) query_info_s;
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");

	info->id = query_id;
	info->listener = listener;

	info->query = "INSERT INTO ";
	info->query = info->query + table_name + " (" + cols + ") VALUES (" + vals + ");";
	info->query = info->query + "SELECT seq FROM sqlite_sequence WHERE name='" + table_name + "';";

	if (!push_thread_event(QTYPE_INSERT, info)) {
		_E("Pushing thread event failed");
		delete info;
		return false;
	}

	return true;
}

bool ctx::db_manager_impl::execute(unsigned int query_id, const char* query, db_listener_iface* listener)
{
	IF_FAIL_RETURN_TAG(initialized, false, _E, "Not initialized");

	query_info_s *info = new(std::nothrow) query_info_s;
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");

	info->id = query_id;
	info->query = query;
	info->listener = listener;

	if (!push_thread_event(QTYPE_EXECUTE, info)) {
		_E("Pushing thread event failed");
		delete info;
		return false;
	}

	return true;
}

void ctx::db_manager_impl::_execute(int query_type, unsigned int query_id, const char* query, db_listener_iface* listener)
{
	_SD("SQL(%d): %s", query_id, query);
	IF_FAIL_VOID(query);
	IF_FAIL_VOID_TAG(db_handle, _E, "DB not opened");

	std::vector<json> *query_result = new(std::nothrow) std::vector<json>;
	IF_FAIL_VOID_TAG(query_result, _E, "Memory allocation failed");

	char *err = NULL;

	int ret = sqlite3_exec(db_handle, query, execution_result_cb, query_result, &err);
	if (ret != SQLITE_OK) {
		_E("DB Error: %s", err);
		sqlite3_free(err);
		send_result(query_type, query_id, listener, ERR_OPERATION_FAILED, query_result);
		return;
	}

	send_result(query_type, query_id, listener, ERR_NONE, query_result);
	return;
}

int ctx::db_manager_impl::execution_result_cb(void *user_data, int dim, char **value, char **column)
{
	std::vector<json> *records = static_cast<std::vector<json>*>(user_data);
	json row;
	bool column_null = false;

	for (int i=0; i<dim; ++i) {
		if (value[i]) {
			row.set(NULL, column[i], value[i]);
		} else {
			column_null = true;
		}
	}

	if (!column_null) {
		records->push_back(row);
	} else {
		_W(RED("Null columns exist"));
	}

	return 0;
}

void ctx::db_manager_impl::send_result(int query_type, unsigned int query_id, db_listener_iface* listener, int error, std::vector<json>* result)
{
	query_result_s *qr = new(std::nothrow) query_result_s();
	IF_FAIL_VOID_TAG(qr, _E, "Memory allocation failed");

	qr->type = query_type;
	qr->id = query_id;
	qr->listener = listener;
	qr->error = error;
	qr->result = result;

	g_idle_add(_send_result, qr);
}

gboolean ctx::db_manager_impl::_send_result(gpointer data)
{
	query_result_s *qr = static_cast<query_result_s*>(data);

	if (qr->listener) {
		switch (qr->type) {
			case QTYPE_CREATE_TABLE:
				qr->listener->on_creation_result_received(qr->id, qr->error);
				break;
			case QTYPE_INSERT:
				{
					int64_t row_id = -1;
					if (qr->error == ERR_NONE && qr->result && !qr->result->empty()) {
						qr->result->at(0).get(NULL, "seq", &row_id);
						_D("RowId: %d", row_id);
					}
					qr->listener->on_insertion_result_received(qr->id, qr->error, row_id);
				}
				break;
			case QTYPE_EXECUTE:
				qr->listener->on_query_result_received(qr->id, qr->error, *(qr->result));
				break;
			default:
				_W("Unknown query type: %d", qr->type);
		}
	}

	delete qr->result;
	delete qr;
	return FALSE;
}

static unsigned int generate_qid()
{
	static GMutex mutex;
	static unsigned int qid = 0;

	ctx::scope_mutex sm(&mutex);

	++qid;

	if (qid == 0) { // Overflow handling
		qid = 1;
	}

	return qid;
}

bool ctx::db_manager_impl::insert_sync(const char* table_name, json& record, int64_t* row_id)
{
	IF_FAIL_RETURN_TAG(initialized, false, _E, "Not initialized");
	IF_FAIL_RETURN_TAG(!db_manager_impl::is_main_thread(), false, _E, "Cannot use this in the main thread");
	IF_FAIL_RETURN_TAG(table_name && row_id, false, _E, "Invalid parameter");

	ctx::scope_mutex sm(&sync_insert_mutex);

	unsigned int qid = generate_qid();
	insert(qid, table_name, record, this);

	while (sync_insert_qid != qid) {
		g_cond_wait(&sync_insert_cond, &sync_insert_mutex);
	}

	*row_id = sync_insert_row_id;

	IF_FAIL_RETURN_TAG(*row_id > 0, false, _E, "Insertion failed");
	return true;
}

void ctx::db_manager_impl::on_insertion_result_received(unsigned int query_id, int error, int64_t row_id)
{
	ctx::scope_mutex sm(&sync_insert_mutex);

	sync_insert_row_id = row_id;
	sync_insert_qid = query_id;

	g_cond_signal(&sync_insert_cond);
}

bool ctx::db_manager_impl::execute_sync(const char* query, std::vector<json>* records)
{
	IF_FAIL_RETURN_TAG(initialized, false, _E, "Not initialized");
	IF_FAIL_RETURN_TAG(!db_manager_impl::is_main_thread(), false, _E, "Cannot use this in the main thread");
	IF_FAIL_RETURN_TAG(query && records, false, _E, "Invalid parameter");

	ctx::scope_mutex sm(&sync_execute_mutex);

	unsigned int qid = generate_qid();
	execute(qid, query, this);

	while (sync_execute_qid != qid) {
		g_cond_wait(&sync_execute_cond, &sync_execute_mutex);
	}

	IF_FAIL_RETURN_TAG(sync_execute_error == ERR_NONE, false, _E, "Query execution failed");

	*records = sync_execute_result;
	sync_execute_result.clear();

	return true;
}

void ctx::db_manager_impl::on_query_result_received(unsigned int query_id, int error, std::vector<json>& records)
{
	ctx::scope_mutex sm(&sync_execute_mutex);

	sync_execute_error = error;
	sync_execute_result = records;
	sync_execute_qid = query_id;

	g_cond_signal(&sync_execute_cond);
}

void ctx::db_manager_impl::on_creation_result_received(unsigned int query_id, int error)
{
	_D("Table Created: QID: %d, Error: %#x", query_id, error);
}
