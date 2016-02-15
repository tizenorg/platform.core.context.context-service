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
#include <ScopeMutex.h>
#include "server.h"
#include "db_mgr_impl.h"

#define CONTEXT_DB_PATH tzplatform_mkpath(TZ_USER_DB, ".context-service.db")

static bool initialized = false;
static GMutex exec_mutex;

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

void ctx::db_manager_impl::onEvent(int type, void* data)
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

	deleteEvent(type, data);
}

void ctx::db_manager_impl::deleteEvent(int type, void* data)
{
	IF_FAIL_VOID(data);
	query_info_s *info = static_cast<query_info_s*>(data);
	delete info;
}

std::string ctx::db_manager_impl::compose_create_query(const char* table_name, const char* columns, const char* option)
{
	std::string query;
	query = "CREATE TABLE IF NOT EXISTS ";
	query = query + table_name + " (row_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT," + columns + ")";
	if (option) {
		query = query + " " + option;
	}
	query += ";";
	return query;
}

bool ctx::db_manager_impl::create_table(unsigned int query_id, const char* table_name, const char* columns, const char* option, db_listener_iface* listener)
{
	IF_FAIL_RETURN_TAG(initialized, false, _E, "Not initialized");

	query_info_s *info = new(std::nothrow) query_info_s;
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");

	info->query = compose_create_query(table_name, columns, option);
	info->id = query_id;
	info->listener = listener;

	if (!pushEvent(QTYPE_CREATE_TABLE, info)) {
		_E("Pushing thread event failed");
		delete info;
		return false;
	}

	return true;
}

std::string ctx::db_manager_impl::compose_insert_query(const char* table_name, json& record)
{
	std::list<std::string> keys;
	IF_FAIL_RETURN_TAG(record.get_keys(&keys), "", _E, "Invalid record");

	std::ostringstream colstream;
	std::ostringstream valstream;

	for (std::list<std::string>::iterator it = keys.begin(); it != keys.end(); ++it) {
		std::string s;
		int64_t i;
		if (record.get(NULL, (*it).c_str(), &s)) {
			colstream << *it << ",";

			char* buf = sqlite3_mprintf("%Q", s.c_str());
			IF_FAIL_RETURN_TAG(buf, "", _E, "Memory allocation failed");
			valstream << buf << ",";
			sqlite3_free(buf);
		} else if (record.get(NULL, (*it).c_str(), &i)) {
			colstream << *it << ",";
			valstream << i << ",";
		}
	}

	std::string cols = colstream.str();
	std::string vals = valstream.str();

	IF_FAIL_RETURN_TAG(!cols.empty(), "", _E, "Invalid record");

	cols.erase(cols.size() - 1);
	vals.erase(vals.size() - 1);

	std::string query = "INSERT INTO ";
	query = query + table_name + " (" + cols + ") VALUES (" + vals + ");";
	query = query + "SELECT seq FROM sqlite_sequence WHERE name='" + table_name + "';";

	return query;
}

bool ctx::db_manager_impl::insert(unsigned int query_id, const char* table_name, json& record, db_listener_iface* listener)
{
	IF_FAIL_RETURN_TAG(initialized, false, _E, "Not initialized");

	std::string query = compose_insert_query(table_name, record);
	IF_FAIL_RETURN(!query.empty(), false);

	query_info_s *info = new(std::nothrow) query_info_s;
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");

	info->query = query;
	info->id = query_id;
	info->listener = listener;

	if (!pushEvent(QTYPE_INSERT, info)) {
		_E("Pushing thread event failed");
		delete info;
		return false;
	}

	return true;
}

bool ctx::db_manager_impl::execute(unsigned int query_id, const char* query, db_listener_iface* listener)
{
	IF_FAIL_RETURN_TAG(initialized, false, _E, "Not initialized");
	IF_FAIL_RETURN_TAG(query, false, _E, "Null query");

	query_info_s *info = new(std::nothrow) query_info_s;
	IF_FAIL_RETURN_TAG(info, false, _E, "Memory allocation failed");

	info->id = query_id;
	info->query = query;
	info->listener = listener;

	if (!pushEvent(QTYPE_EXECUTE, info)) {
		_E("Pushing thread event failed");
		delete info;
		return false;
	}

	return true;
}

void ctx::db_manager_impl::_execute(int query_type, unsigned int query_id, const char* query, db_listener_iface* listener)
{
	IF_FAIL_VOID(query);
	IF_FAIL_VOID_TAG(db_handle, _E, "DB not opened");
	_SD("SQL(%d): %s", query_id, query);

	std::vector<json> *query_result = new(std::nothrow) std::vector<json>;
	IF_FAIL_VOID_TAG(query_result, _E, "Memory allocation failed");

	char *err = NULL;
	int ret;

	{
		ScopeMutex sm(&exec_mutex);
		ret = sqlite3_exec(db_handle, query, execution_result_cb, query_result, &err);
	}

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

bool ctx::db_manager_impl::create_table_sync(const char* table_name, const char* columns, const char* option)
{
	IF_FAIL_RETURN_TAG(db_handle, false, _E, "DB not opened");

	std::string query = compose_create_query(table_name, columns, option);
	IF_FAIL_RETURN(!query.empty(), false);
	_SD("SQL: %s", query.c_str());

	char *err = NULL;
	int ret;
	{
		ScopeMutex sm(&exec_mutex);
		ret = sqlite3_exec(db_handle, query.c_str(), NULL, NULL, &err);
	}

	if (ret != SQLITE_OK) {
		_E("DB Error: %s", err);
		sqlite3_free(err);
		return false;
	}

	return true;
}

bool ctx::db_manager_impl::insert_sync(const char* table_name, json& record, int64_t* row_id)
{
	IF_FAIL_RETURN_TAG(db_handle, false, _E, "DB not opened");
	IF_FAIL_RETURN_TAG(table_name && row_id, false, _E, "Invalid parameter");

	std::string query = compose_insert_query(table_name, record);
	IF_FAIL_RETURN(!query.empty(), false);
	_SD("SQL: %s", query.c_str());

	std::vector<json> query_result;
	char *err = NULL;
	int ret;
	{
		ScopeMutex sm(&exec_mutex);
		ret = sqlite3_exec(db_handle, query.c_str(), execution_result_cb, &query_result, &err);
	}

	if (ret != SQLITE_OK) {
		_E("DB Error: %s", err);
		sqlite3_free(err);
		return false;
	}

	IF_FAIL_RETURN_TAG(!query_result.empty(), false, _E, "No row id");

	*row_id = -1;
	query_result.at(0).get(NULL, "seq", row_id);
	_D("RowId: %lld", *row_id);

	return true;
}

bool ctx::db_manager_impl::execute_sync(const char* query, std::vector<json>* records)
{
	IF_FAIL_RETURN_TAG(db_handle, false, _E, "DB not opened");
	IF_FAIL_RETURN_TAG(query && records, false, _E, "Invalid parameter");

	_SD("SQL: %s", query);

	char *err = NULL;
	int ret;
	{
		ScopeMutex sm(&exec_mutex);
		ret = sqlite3_exec(db_handle, query, execution_result_cb, records, &err);
	}

	if (ret != SQLITE_OK) {
		_E("DB Error: %s", err);
		sqlite3_free(err);
		return false;
	}

	return true;
}
