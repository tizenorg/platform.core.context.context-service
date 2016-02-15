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

#include <sstream>
#include <json.h>
#include <context_trigger_types_internal.h>
#include <db_mgr.h>
#include <package_manager.h>
#include "rule_manager.h"
#include "template_manager.h"
#include "context_monitor.h"
#include "rule.h"
#include "timer.h"

#define RULE_TABLE "context_trigger_rule"

static int string_to_int(std::string str)
{
	int i;
	std::istringstream convert(str);

	if (!(convert >> i))
		i = 0;

	return i;
}

static std::string int_to_string(int i)
{
	std::ostringstream convert;
	convert << i;
	std::string str = convert.str();
	return str;
}

ctx::rule_manager::rule_manager()
{
}

ctx::rule_manager::~rule_manager()
{
}

bool ctx::rule_manager::init()
{
	bool ret;
	int error;

	// Create tables into db (rule, template)
	std::string q1 = std::string("status INTEGER DEFAULT 0 NOT NULL, creator TEXT DEFAULT '' NOT NULL,")
			+ "package_id TEXT DEFAULT '' NOT NULL, description TEXT DEFAULT '',"
			+ "details TEXT DEFAULT '' NOT NULL";
	ret = db_manager::create_table(1, RULE_TABLE, q1.c_str(), NULL, NULL);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Create rule table failed");

	// Before re-enable rules, handle uninstalled app's rules
	if (get_uninstalled_app() > 0) {
		error = clear_rule_of_uninstalled_package(true);
		IF_FAIL_RETURN_TAG(error == ERR_NONE, false, _E, "Failed to remove uninstalled apps' rules while initialization");
	}
	ret = reenable_rule();

	return ret;
}

void ctx::rule_manager::handle_rule_of_uninstalled_package(std::string pkg_id)
{
	uninstalled_packages.insert(pkg_id);
	clear_rule_of_uninstalled_package();
}

int ctx::rule_manager::get_uninstalled_app(void)
{
	// Return number of uninstalled apps
	std::string q1 = "SELECT DISTINCT package_id FROM context_trigger_rule";

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q1.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, -1, _E, "Query package ids of registered rules failed");

	std::vector<json>::iterator vec_end = record.end();
	for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		std::string pkg_id;
		elem.get(NULL, "package_id", &pkg_id);

		if (is_uninstalled_package(pkg_id)) {
			uninstalled_packages.insert(pkg_id);
		}
	}

	return uninstalled_packages.size();
}

bool ctx::rule_manager::is_uninstalled_package(std::string pkg_id)
{
	IF_FAIL_RETURN_TAG(!pkg_id.empty(), false, _D, "Empty package id");

	package_info_h pkg_info;
	int error = package_manager_get_package_info(pkg_id.c_str(), &pkg_info);

	if (error == PACKAGE_MANAGER_ERROR_NONE) {
		package_info_destroy(pkg_info);
	} else if (error == PACKAGE_MANAGER_ERROR_NO_SUCH_PACKAGE) {
		// Uninstalled package found
		_D("Uninstalled package found: %s", pkg_id.c_str());
		return true;
	} else {
		_E("Failed to get package info(%s): %d", pkg_id.c_str(), error);
	}

	return false;
}

int ctx::rule_manager::clear_rule_of_uninstalled_package(bool is_init)
{
	if (uninstalled_packages.size() <= 0) {
		return ERR_NONE;
	}

	int error;
	bool ret;

	_D("Clear uninstalled packages' rule started");
	// Package list
	std::string pkg_list = "(";
	std::set<std::string>::iterator it = uninstalled_packages.begin();
	pkg_list += "package_id = '" + *it + "'";
	it++;
	for (; it != uninstalled_packages.end(); ++it) {
		pkg_list += " OR package_id = '" + *it + "'";
	}
	pkg_list += ")";

	// After event received, disable all the enabled rules of uninstalled apps
	if (!is_init) {
		std::string q1 = "SELECT row_id FROM context_trigger_rule WHERE status = 2 and (";
		q1 += pkg_list;
		q1 += ")";

		std::vector<json> record;
		ret = db_manager::execute_sync(q1.c_str(), &record);
		IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Failed to query enabled rules of uninstalled packages");

		std::vector<json>::iterator vec_end = record.end();
		for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
			ctx::json elem = *vec_pos;
			int rule_id;
			elem.get(NULL, "row_id", &rule_id);
			error = disable_rule(rule_id);
			IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to disable rule" );
		}
		_D("Uninstalled packages' rules are disabled");
	}

	// Delete rules of uninstalled packages from DB
	std::string q2 = "DELETE FROM context_trigger_rule WHERE " + pkg_list;
	std::vector<json> dummy;
	ret = db_manager::execute_sync(q2.c_str(), &dummy);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Failed to remove rules from db");
	_D("Uninstalled packages' rules are deleted from db");

	uninstalled_packages.clear();

	return ERR_NONE;
}

int ctx::rule_manager::pause_rule_with_item(std::string& subject)
{
	std::string q = "SELECT row_id FROM context_trigger_rule WHERE (status=2) AND (details LIKE '%\"ITEM_NAME\":\"" + subject + "\"%');";
	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Failed to query row_ids to be paused");
	IF_FAIL_RETURN(record.size() > 0, ERR_NONE);

	_D("Pause rules related to %s", subject.c_str());
	std::vector<json>::iterator vec_end = record.end();
	for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		int row_id;
		elem.get(NULL, "row_id", &row_id);

		int error = pause_rule(row_id);
		IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to disable rules using custom item");
	}

	return ERR_NONE;
}

int ctx::rule_manager::resume_rule_with_item(std::string& subject)
{
	std::string q = "SELECT row_id FROM context_trigger_rule WHERE (status=1) AND (details LIKE '%\"ITEM_NAME\":\"" + subject + "\"%');";
	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query paused rule ids failed");
	IF_FAIL_RETURN(record.size() > 0, ERR_NONE);

	_D("Resume rules related to %s", subject.c_str());
	std::string q_rowid;
	std::vector<json>::iterator vec_end = record.end();
	for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		int row_id;
		elem.get(NULL, "row_id", &row_id);

		int error = enable_rule(row_id);
		IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to resume rule");
	}

	return ERR_NONE;
}

bool ctx::rule_manager::reenable_rule(void)
{
	int error;
	std::string q = "SELECT row_id FROM context_trigger_rule WHERE status = 2";

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query row_ids of enabled rules failed");
	IF_FAIL_RETURN_TAG(record.size() > 0, true, _D, "No rule to re-enable");

	_D(YELLOW("Re-enable rule started"));

	std::string q_rowid;
	q_rowid.clear();
	std::vector<json>::iterator vec_end = record.end();
	for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		int row_id;
		elem.get(NULL, "row_id", &row_id);

		error = enable_rule(row_id);
		if (error == ERR_NOT_SUPPORTED) {
			q_rowid += "(row_id = " + int_to_string(row_id) + ") OR ";
		} else if (error != ERR_NONE) {
			_E("Re-enable rule%d failed(%d)", row_id, error);
		}
	}
	IF_FAIL_RETURN(!q_rowid.empty(), true);
	q_rowid = q_rowid.substr(0, q_rowid.length() - 4);

	// For rules which is failed to re-enable
	std::string q_update = "UPDATE context_trigger_rule SET status = 1 WHERE " + q_rowid;
	std::vector<json> record2;
	ret = db_manager::execute_sync(q_update.c_str(), &record2);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Failed to update rules as paused");

	return true;
}

bool ctx::rule_manager::rule_data_arr_elem_equals(ctx::json& lelem, ctx::json& relem)
{
	std::string lkey, rkey;
	lelem.get(NULL, CT_RULE_DATA_KEY, &lkey);
	relem.get(NULL, CT_RULE_DATA_KEY, &rkey);
	if (lkey.compare(rkey))
		return false;

	int lvc, rvc, lvoc, rvoc;
	lvc = lelem.array_get_size(NULL, CT_RULE_DATA_VALUE_ARR);
	rvc = relem.array_get_size(NULL, CT_RULE_DATA_VALUE_ARR);
	lvoc = lelem.array_get_size(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR);
	rvoc = relem.array_get_size(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR);
	if (!((lvc == rvc) && (lvc == lvoc) && (lvc && rvoc)))
		return false;

	if (lvc > 1) {
		std::string lop, rop;
		lelem.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &lop);
		relem.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &rop);
		if (lop.compare(rop))
			return false;
	}

	for (int i = 0; i < lvc; i++) {
		bool found = false;
		std::string lv, lvo;
		lelem.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, i, &lv);
		lelem.get_array_elem(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, i, &lvo);

		for (int j = 0; j < lvc; j++) {
			std::string rv, rvo;
			relem.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, j, &rv);
			relem.get_array_elem(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, j, &rvo);

			if (!lv.compare(rv) && !lvo.compare(rvo)) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	return true;
}

bool ctx::rule_manager::rule_item_equals(ctx::json& litem, ctx::json& ritem)
{
	// Compare item name
	std::string lei, rei;
	litem.get(NULL, CT_RULE_EVENT_ITEM, &lei);
	ritem.get(NULL, CT_RULE_EVENT_ITEM, &rei);
	if (lei.compare(rei))
		return false;

	// Compare option
	ctx::json loption, roption;
	litem.get(NULL, CT_RULE_EVENT_OPTION, &loption);
	ritem.get(NULL, CT_RULE_EVENT_OPTION, &roption);
	if (loption != roption)
		return false;

	int ledac, redac;
	ledac = litem.array_get_size(NULL, CT_RULE_DATA_ARR);
	redac = ritem.array_get_size(NULL, CT_RULE_DATA_ARR);
	if (ledac != redac)
		return false;

	// Compare item operator;
	if (ledac > 1 ) {
		std::string leop, reop;
		litem.get(NULL, CT_RULE_EVENT_OPERATOR, &leop);
		ritem.get(NULL, CT_RULE_EVENT_OPERATOR, &reop);
		if (leop.compare(reop))
			return false;
	}

	for (int i = 0; i < ledac; i++) {
		bool found = false;
		ctx::json lelem;
		litem.get_array_elem(NULL, CT_RULE_DATA_ARR, i, &lelem);

		for (int j = 0; j < ledac; j++) {
			ctx::json relem;
			ritem.get_array_elem(NULL, CT_RULE_DATA_ARR, j, &relem);

			if (rule_data_arr_elem_equals(lelem, relem)) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	return true;
}

bool ctx::rule_manager::rule_equals(ctx::json& lrule, ctx::json& rrule)
{
	// Compare event
	ctx::json le, re;
	lrule.get(NULL, CT_RULE_EVENT, &le);
	rrule.get(NULL, CT_RULE_EVENT, &re);
	if (!rule_item_equals(le, re))
		return false;

	// Compare conditions
	int lcc, rcc;
	lcc = lrule.array_get_size(NULL, CT_RULE_CONDITION);
	rcc = rrule.array_get_size(NULL, CT_RULE_CONDITION);
	if (lcc != rcc)
		return false;

	if (lcc > 1) {
		std::string lop, rop;
		lrule.get(NULL, CT_RULE_OPERATOR, &lop);
		rrule.get(NULL, CT_RULE_OPERATOR, &rop);
		if (lop.compare(rop))
			return false;
	}

	for (int i = 0; i < lcc; i++) {
		bool found = false;
		ctx::json lc;
		lrule.get_array_elem(NULL, CT_RULE_CONDITION, i, &lc);

		for (int j = 0; j < lcc; j++) {
			ctx::json rc;
			rrule.get_array_elem(NULL, CT_RULE_CONDITION, j, &rc);

			if (rule_item_equals(lc, rc)) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	// Compare action
	ctx::json laction, raction;
	lrule.get(NULL, CT_RULE_ACTION, &laction);
	rrule.get(NULL, CT_RULE_ACTION, &raction);
	if (laction != raction)
		return false;

	return true;
}

int64_t ctx::rule_manager::get_duplicated_rule_id(std::string pkg_id, ctx::json& rule)
{
	std::string q = "SELECT row_id, description, details FROM context_trigger_rule WHERE package_id = '";
	q += pkg_id;
	q += "'";

	std::vector<json> d_record;
	bool ret = db_manager::execute_sync(q.c_str(), &d_record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query row_id, details by package id failed");

	ctx::json r_details;
	rule.get(NULL, CT_RULE_DETAILS, &r_details);
	std::string r_desc;
	rule.get(NULL, CT_RULE_DESCRIPTION, &r_desc);
	std::vector<json>::iterator vec_end = d_record.end();

	for (std::vector<json>::iterator vec_pos = d_record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		std::string details;
		ctx::json d_details;

		elem.get(NULL, "details", &details);
		d_details = details;

		if (rule_equals(r_details, d_details)) {
			int64_t row_id;
			elem.get(NULL, "row_id", &row_id);

			// Description comparison
			std::string d_desc;
			elem.get(NULL, "description", &d_desc);
			if (r_desc.compare(d_desc)) {
				// Only description is changed
				std::string q_update = "UPDATE context_trigger_rule SET description='" + r_desc + "' WHERE row_id = " + int_to_string(row_id);

				std::vector<json> record;
				ret = db_manager::execute_sync(q_update.c_str(), &record);
				if (ret) {
					_D("Rule%lld description is updated", row_id);
				} else {
					_W("Failed to update description of rule%lld", row_id);
				}
			}

			return row_id;
		}
	}

	return -1;
}

int ctx::rule_manager::verify_rule(ctx::json& rule, const char* creator)
{
	ctx::json details;
	rule.get(NULL, CT_RULE_DETAILS, &details);

	std::string e_name;
	rule.get(CT_RULE_DETAILS "." CT_RULE_EVENT, CT_RULE_EVENT_ITEM, &e_name);

	ctx::context_monitor* ctx_monitor = ctx::context_monitor::get_instance();
	IF_FAIL_RETURN_TAG(ctx_monitor, ERR_OUT_OF_MEMORY, _E, "Memory allocation failed");

	IF_FAIL_RETURN_TAG(ctx_monitor->is_supported(e_name), ERR_NOT_SUPPORTED, _I, "Event(%s) is not supported", e_name.c_str());

	if (creator) {
		if (!ctx_monitor->is_allowed(creator, e_name.c_str())) {
			_W("Permission denied for '%s'", e_name.c_str());
			return ERR_PERMISSION_DENIED;
		}
	}

	ctx::json it;
	for (int i = 0; rule.get_array_elem(CT_RULE_DETAILS, CT_RULE_CONDITION, i, &it); i++){
		std::string c_name;
		it.get(NULL, CT_RULE_CONDITION_ITEM, &c_name);

		IF_FAIL_RETURN_TAG(ctx_monitor->is_supported(c_name), ERR_NOT_SUPPORTED, _I, "Condition(%s) is not supported", c_name.c_str());

		if (!ctx_monitor->is_allowed(creator, c_name.c_str())) {
			_W("Permission denied for '%s'", c_name.c_str());
			return ERR_PERMISSION_DENIED;
		}
	}

	return ERR_NONE;
}

int ctx::rule_manager::add_rule(std::string creator, const char* pkg_id, ctx::json rule, ctx::json* rule_id)
{
	apply_templates();
	bool ret;
	int64_t rid;

	// Check if all items are supported && allowed to access
	int err = verify_rule(rule, creator.c_str());
	IF_FAIL_RETURN(err == ERR_NONE, err);

	// Check if duplicated rule exits
	if ((rid = get_duplicated_rule_id(pkg_id, rule)) > 0) {
		// Save rule id
		rule_id->set(NULL, CT_RULE_ID, rid);
		_D("Duplicated rule found");
		return ERR_NONE;
	}

	// Insert rule to rule table, get rule id
	ctx::json r_record;
	std::string description;
	ctx::json details;
	rule.get(NULL, CT_RULE_DESCRIPTION, &description);
	rule.get(NULL, CT_RULE_DETAILS, &details);
	r_record.set(NULL, "creator", creator);
	if (pkg_id) {
		r_record.set(NULL, "package_id", pkg_id);
	}
	r_record.set(NULL, "description", description);

	// Handle timer event
	ctx::trigger_timer::handle_timer_event(details);

	r_record.set(NULL, "details", details.str());
	ret = db_manager::insert_sync(RULE_TABLE, r_record, &rid);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Insert rule to db failed");

	// Save rule id
	rule_id->set(NULL, CT_RULE_ID, rid);

	_D("Add rule%d succeeded", (int)rid);
	return ERR_NONE;
}


int ctx::rule_manager::remove_rule(int rule_id)
{
	apply_templates();
	bool ret;

	// Delete rule from DB
	std::string query = "DELETE FROM 'context_trigger_rule' where row_id = ";
	query += int_to_string(rule_id);

	std::vector<json> record;
	ret = db_manager::execute_sync(query.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Remove rule from db failed");

	_D("Remove rule%d succeeded", rule_id);

	return ERR_NONE;
}

int ctx::rule_manager::enable_rule(int rule_id)
{
	apply_templates();
	int error;
	std::string query;
	std::vector<json> rule_record;
	std::vector<json> record;
	std::string pkg_id;
	ctx::json jrule;
	std::string tmp;
	std::string id_str = int_to_string(rule_id);

	trigger_rule* rule;

	// Get rule json by rule id;
	query = "SELECT details, package_id FROM context_trigger_rule WHERE row_id = ";
	query += id_str;
	error = (db_manager::execute_sync(query.c_str(), &rule_record))? ERR_NONE : ERR_OPERATION_FAILED;
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Query rule by rule id failed");

	rule_record[0].get(NULL, "details", &tmp);
	jrule = tmp;
	rule_record[0].get(NULL, "package_id", &pkg_id);

	// Create a rule instance
	rule = new(std::nothrow) trigger_rule(rule_id, jrule, pkg_id.c_str(), this);
	IF_FAIL_RETURN_TAG(rule, ERR_OUT_OF_MEMORY, _E, "Failed to create rule instance");

	// Start the rule
	error = rule->start();
	IF_FAIL_CATCH_TAG(error == ERR_NONE, _E, "Failed to start rule%d", rule_id);

	// Update db to set 'enabled'
	query = "UPDATE context_trigger_rule SET status = 2 WHERE row_id = ";
	query += id_str;
	error = (db_manager::execute_sync(query.c_str(), &record))? ERR_NONE : ERR_OPERATION_FAILED;
	IF_FAIL_CATCH_TAG(error == ERR_NONE, _E, "Update db failed");

	// Add rule instance to rule_map
	rule_map[rule_id] = rule;

	_D(YELLOW("Enable Rule%d succeeded"), rule_id);
	return ERR_NONE;

CATCH:
	delete rule;
	rule = NULL;

	return error;
}

int ctx::rule_manager::disable_rule(int rule_id)
{
	apply_templates();
	bool ret;
	int error;

	rule_map_t::iterator it = rule_map.find(rule_id);
	bool is_paused = (it == rule_map.end());

	// For 'enabled' rule, not 'paused'
	if (!is_paused) {
		// Stop the rule
		trigger_rule* rule = static_cast<trigger_rule*>(it->second);
		error = rule->stop();
		IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to stop rule%d", rule_id);

		// Remove rule instance from rule_map
		delete rule;
		rule_map.erase(it);
	}

	// Update db to set 'disabled'	// TODO skip while clear uninstalled rule
	std::string query = "UPDATE context_trigger_rule SET status = 0 WHERE row_id = ";
	query += int_to_string(rule_id);
	std::vector<json> record;
	ret = db_manager::execute_sync(query.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Update db failed");

	_D(YELLOW("Disable Rule%d succeeded"), rule_id);
	return ERR_NONE;
}

int ctx::rule_manager::pause_rule(int rule_id)
{
	bool ret;
	int error;

	rule_map_t::iterator it = rule_map.find(rule_id);
	IF_FAIL_RETURN_TAG(it != rule_map.end(), ERR_OPERATION_FAILED, _E, "Rule instance not found");

	// Stop the rule
	trigger_rule* rule = static_cast<trigger_rule*>(it->second);
	error = rule->stop();
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to stop rule%d", rule_id);

	// Update db to set 'paused'
	std::string query = "UPDATE context_trigger_rule SET status = 1 WHERE row_id = ";

	query += int_to_string(rule_id);
	std::vector<json> record;
	ret = db_manager::execute_sync(query.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Update db failed");

	// Remove rule instance from rule_map
	delete rule;
	rule_map.erase(it);

	_D(YELLOW("Pause Rule%d"), rule_id);
	return ERR_NONE;
}

int ctx::rule_manager::check_rule(std::string pkg_id, int rule_id)
{
	// Get package id
	std::string q = "SELECT package_id FROM context_trigger_rule WHERE row_id =";
	q += int_to_string(rule_id);

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query package id by rule id failed");

	if (record.size() == 0) {
		return ERR_NO_DATA;
	}

	std::string p;
	record[0].get(NULL, "package_id", &p);

	if (p.compare(pkg_id) == 0){
		return ERR_NONE;
	}

	return ERR_NO_DATA;
}

bool ctx::rule_manager::is_rule_enabled(int rule_id)
{
	std::string q = "SELECT status FROM context_trigger_rule WHERE row_id =";
	q += int_to_string(rule_id);

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query enabled by rule id failed");

	int status;
	record[0].get(NULL, "status", &status);

	return (status != 0);
}

int ctx::rule_manager::get_rule_by_id(std::string pkg_id, int rule_id, ctx::json* request_result)
{
	apply_templates();
	std::string q = "SELECT description FROM context_trigger_rule WHERE (package_id = '";
	q += pkg_id;
	q += "') and (row_id = ";
	q += int_to_string(rule_id);
	q += ")";

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query rule by rule id failed");

	if (record.size() == 0) {
		return ERR_NO_DATA;
	} else if (record.size() != 1) {
		return ERR_OPERATION_FAILED;
	}

	std::string description;
	record[0].get(NULL, "description", &description);

	(*request_result).set(NULL, CT_RULE_ID, rule_id);
	(*request_result).set(NULL, CT_RULE_DESCRIPTION, description);

	return ERR_NONE;
}

int ctx::rule_manager::get_rule_ids(std::string pkg_id, ctx::json* request_result)
{
	(*request_result) = "{ \"" CT_RULE_ARRAY_ENABLED "\" : [ ] , \"" CT_RULE_ARRAY_DISABLED "\" : [ ] }";

	std::string q = "SELECT row_id, status FROM context_trigger_rule WHERE (package_id = '";
	q += pkg_id;
	q += "')";

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query rules failed");

	std::vector<json>::iterator vec_end = record.end();
	for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		std::string id;
		int status;

		elem.get(NULL, "row_id", &id);
		elem.get(NULL, "status", &status);

		if (status >= 1) {
			(*request_result).array_append(NULL, CT_RULE_ARRAY_ENABLED, string_to_int(id));
		} else if (status == 0) {
			(*request_result).array_append(NULL, CT_RULE_ARRAY_DISABLED, string_to_int(id));
		}
	}

	return ERR_NONE;
}

void ctx::rule_manager::apply_templates()
{
	ctx::template_manager* tmpl_mgr = ctx::template_manager::get_instance();
	IF_FAIL_VOID_TAG(tmpl_mgr, _E, "Memory allocation failed");
	tmpl_mgr->apply_templates();
}
