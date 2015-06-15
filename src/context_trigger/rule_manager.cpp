/*
 * context-service
 *
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
 *
 */

#include <sstream>
#include <app.h>
#include <glib.h>
#include <types_internal.h>
#include <json.h>
#include <stdlib.h>
#include <bundle.h>
#include <app_control.h>
#include <appsvc.h>
#include <app_control_internal.h>
#include <notification.h>
#include <notification_internal.h>
#include <context_trigger_types_internal.h>
#include <context_trigger.h>
#include <db_mgr.h>
#include <zone_util.h>
#include "../access_control/privilege.h"
#include "rule_manager.h"
#include "script_generator.h"
#include "trigger.h"

#define RULE_TABLE "context_trigger_rule"
#define EVENT_TABLE "context_trigger_event"
#define CONDITION_TABLE "context_trigger_condition"
#define TEMPLATE_TABLE "context_trigger_template"

#define RULE_TABLE_COLUMNS "enabled INTEGER DEFAULT 0 NOT NULL, creator TEXT DEFAULT '' NOT NULL, description TEXT DEFAULT '', details TEXT DEFAULT '' NOT NULL, zone TEXT DEFAULT '' NOT NULL"
#define EVENT_TABLE_COLUMNS "rule_id INTEGER references context_trigger_rule(row_id) ON DELETE CASCADE NOT NULL, name TEXT DEFAULT '' NOT NULL, instance_name TEXT DEFAULT '', zone TEXT DEFAULT '' NOT NULL"
#define CONDITION_TABLE_COLUMNS "rule_id INTEGER references context_trigger_rule(row_id) ON DELETE CASCADE NOT NULL, name TEXT DEFAULT '' NOT NULL, option TEXT DEFAULT '', instance_name TEXT DEFAULT '', zone TEXT DEFAULT '' NOT NULL"
#define CREATE_TEMPLATE_TABLE "CREATE TABLE IF NOT EXISTS context_trigger_template (name TEXT DEFAULT '' NOT NULL PRIMARY KEY, j_template TEXT DEFAULT '' NOT NULL)"
#define QUERY_TEMPLATE_TABLE "SELECT j_template FROM context_trigger_template"
#define FOREIGN_KEYS_ON "PRAGMA foreign_keys = ON"
#define DELETE_RULE_STATEMENT "DELETE FROM 'context_trigger_rule' where row_id = "
#define UPDATE_RULE_ENABLED_STATEMENT "UPDATE context_trigger_rule SET enabled = 1 WHERE row_id = "
#define UPDATE_RULE_DISABLED_STATEMENT "UPDATE context_trigger_rule SET enabled = 0 WHERE row_id = "
#define QUERY_NAME_INSTANCE_NAME_AND_TEMPLATE_BY_RULE_ID_STATEMENT "SELECT context_trigger_condition.name, instance_name, j_template as templates FROM context_trigger_condition JOIN context_trigger_template ON (context_trigger_condition.name = context_trigger_template.name) WHERE rule_id = "
#define QUERY_CONDITION_TEMPLATES_OF_INVOKED_EVENT_STATEMENT "SELECT DISTINCT context_trigger_condition.name, instance_name, option, j_template FROM context_trigger_condition JOIN context_trigger_template ON (context_trigger_condition.name = context_trigger_template.name) WHERE rule_id IN (SELECT row_id FROM context_trigger_rule WHERE enabled = 1 AND row_id IN (SELECT rule_id FROM context_trigger_event WHERE context_trigger_event.instance_name = '"
#define QUERY_RULE_AND_ZONE_BY_RULE_ID "SELECT details, zone FROM context_trigger_rule WHERE row_id = "
#define QUERY_EVENT_TEMPLATE_BY_RULE_ID "SELECT j_template FROM context_trigger_template WHERE name IN (SELECT name FROM context_trigger_event WHERE rule_id = "
#define QUERY_CONDITION_BY_RULE_ID "SELECT name, option FROM context_trigger_condition WHERE rule_id = "

#define INSTANCE_NAME_DELIMITER "/"
#define EVENT_KEY_PREFIX "?"

static ctx::context_trigger* trigger = NULL;

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

bool ctx::rule_manager::init(ctx::context_trigger* tr, ctx::fact_reader* fr)
{
	int error;
	bool ret;

	trigger = tr;

	ret = clips_h.init(this);
	IF_FAIL_RETURN_TAG(ret, false, _E, "CLIPS handler initialization failed");

	ret = c_monitor.init(fr, tr);

	// Create tables into db (rule, event, condition, action, template)
	ret = db_manager::create_table(1, RULE_TABLE, RULE_TABLE_COLUMNS, NULL, NULL);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Create rule table failed");

	ret = db_manager::create_table(2, EVENT_TABLE, EVENT_TABLE_COLUMNS, NULL, NULL);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Create event table failed");

	ret = db_manager::create_table(3, CONDITION_TABLE, CONDITION_TABLE_COLUMNS, NULL, NULL);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Create condition table failed");

	ret = db_manager::execute(4, CREATE_TEMPLATE_TABLE, NULL);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Create template table failed");

	// Load all templates from DB
	std::vector<json> record;
	ret = db_manager::execute_sync(QUERY_TEMPLATE_TABLE, &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query template table failed");

	// Make scripts for deftemplate, defclass, make-instance and load them to clips
	std::vector<json>::iterator vec_end = record.end();
	for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		std::string tmpl_str;
		elem.get(NULL, "j_template", &tmpl_str);
		ctx::json tmpl = tmpl_str;

		std::string deftemplate_str = script_generator::generate_deftemplate(&tmpl);
		error = clips_h.define_template(deftemplate_str);
		IF_FAIL_RETURN_TAG(error == ERR_NONE, false, _E, "Deftemplate failed");

		std::string defclass_str = script_generator::generate_defclass(&tmpl);
		error = clips_h.define_class(defclass_str);
		IF_FAIL_RETURN_TAG(error == ERR_NONE, false, _E, "Defclass failed");

		std::string makeinstance_str = script_generator::generate_makeinstance(&tmpl);
		error = clips_h.make_instance(makeinstance_str);
		IF_FAIL_RETURN_TAG(error == ERR_NONE, false, _E, "Makeinstance failed");
	}

	// Foreign keys on
	ret = db_manager::execute_sync(FOREIGN_KEYS_ON, &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Foreign keys on failed");

	ret = reenable_rule();

	return ret;
}

bool ctx::rule_manager::reenable_rule(void)
{
	int error;
	std::string q = "SELECT row_id FROM context_trigger_rule where enabled = 1";

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query row_ids of enabled rules failed");

	std::vector<json>::iterator vec_end = record.end();
	for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		int row_id;
		elem.get(NULL, "row_id", &row_id);

		error = enable_rule(row_id);
		if (error != ERR_NONE) {
			_E("Re-enable rule%d failed(%d)", row_id, error);
		} else {
			_E("Re-enable rule%d succeeded", row_id);
		}
	}

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
	std::string linst, rinst;
	litem.get(NULL, CT_RULE_EVENT_OPTION, &loption);
	ritem.get(NULL, CT_RULE_EVENT_OPTION, &roption);
	linst = get_instance_name(lei, loption);
	rinst = get_instance_name(rei, roption);
	if (linst.compare(rinst))
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
	std::string laction, raction;
	lrule.get(NULL, CT_RULE_ACTION, &laction);
	rrule.get(NULL, CT_RULE_ACTION, &raction);
	if (laction.compare(raction))
		return false;

	return true;
}

int64_t ctx::rule_manager::get_duplicated_rule(std::string creator, std::string zone, ctx::json& rule)
{
	std::string q = "SELECT row_id, details FROM context_trigger_rule WHERE creator = '";
	q += creator;
	q += "' AND zone ='";
	q += zone;
	q += "'";

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query row_id, details by creator failed");

	ctx::json r_details;
	rule.get(NULL, CT_RULE_DETAILS, &r_details);
	std::vector<json>::iterator vec_end = record.end();

	for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		std::string details;
		ctx::json d_details;

		elem.get(NULL, "details", &details);
		d_details = details;

		if (rule_equals(r_details, d_details)) {
			int64_t row_id;
			elem.get(NULL, "row_id", &row_id);
			return row_id;
		}
	}

	return -1;
}

int ctx::rule_manager::verify_rule(ctx::json& rule, const char* app_id, const char* zone)
{
	ctx::json details;
	rule.get(NULL, CT_RULE_DETAILS, &details);

	std::string e_name;
	rule.get(CT_RULE_DETAILS "." CT_RULE_EVENT, CT_RULE_EVENT_ITEM, &e_name);

	IF_FAIL_RETURN_TAG(c_monitor.is_supported(e_name, zone), ERR_NOT_SUPPORTED, _I, "Event(%s) is not supported", e_name.c_str());

	if (app_id) {
		if (!ctx::privilege_manager::is_allowed(app_id, e_name.c_str())) {
			_W("Permission denied for '%s'", e_name.c_str());
			return ERR_PERMISSION_DENIED;
		}
	}

	ctx::json it;
	for (int i = 0; rule.get_array_elem(CT_RULE_DETAILS, CT_RULE_CONDITION, i, &it); i++){
		std::string c_name;
		it.get(NULL, CT_RULE_CONDITION_ITEM, &c_name);

		IF_FAIL_RETURN_TAG(c_monitor.is_supported(c_name, zone), ERR_NOT_SUPPORTED, _I, "Condition(%s) is not supported", c_name.c_str());

		if (!ctx::privilege_manager::is_allowed(app_id, c_name.c_str())) {
			_W("Permission denied for '%s'", c_name.c_str());
			return ERR_PERMISSION_DENIED;
		}
	}

	return ERR_NONE;
}

int ctx::rule_manager::add_rule(std::string creator, ctx::json rule, std::string zone, ctx::json* rule_id)
{
	// * Insert rule to DB
	bool ret;
	int64_t rid;

	// Check if all items are supported && allowed to access
	int err = verify_rule(rule, creator.c_str(), zone.c_str());
	IF_FAIL_RETURN(err==ERR_NONE, err);

	// Check if duplicated rule exits
	if ((rid = get_duplicated_rule(creator, zone, rule)) > 0) {
		// Save rule id
		rule_id->set(NULL, CT_RULE_ID, rid);
		return ERR_NONE;
	}

	// Insert rule to rule table, get rule id and save it to json parameter
	ctx::json r_record;
	std::string description;
	ctx::json details;
	rule.get(NULL, CT_RULE_DESCRIPTION, &description);
	rule.get(NULL, CT_RULE_DETAILS, &details);
	r_record.set(NULL, "creator", creator);
	r_record.set(NULL, "description", description);
	r_record.set(NULL, "details", details.str());
	r_record.set(NULL, "zone", zone);
	ret = db_manager::insert_sync(RULE_TABLE, r_record, &rid);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Insert rule to db failed");

	// Save rule id
	rule_id->set(NULL, CT_RULE_ID, rid);

	// Insert event & conditions of a rule into each table
	ctx::json e_record;
	std::string e_name;
	ctx::json e_option_j;
	std::string e_inst;

	rule.get(CT_RULE_DETAILS "." CT_RULE_EVENT, CT_RULE_EVENT_ITEM, &e_name);
	rule.get(CT_RULE_DETAILS "." CT_RULE_EVENT, CT_RULE_EVENT_OPTION, &e_option_j);
	e_inst = get_instance_name(e_name, e_option_j);

	e_record.set(NULL, "rule_id", rid);
	e_record.set(NULL, "name", e_name);
	e_record.set(NULL, "instance_name", e_inst);
	e_record.set(NULL, "zone", zone);
	ret = db_manager::insert(1, EVENT_TABLE, e_record, NULL);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Insert event to db failed");

	ctx::json it;
	for (int i = 0; rule.get_array_elem(CT_RULE_DETAILS, CT_RULE_CONDITION, i, &it); i++){
		ctx::json c_record;
		std::string c_name;
		ctx::json c_option;
		char* c_option_str;
		ctx::json tmp_option;
		std::string c_inst;

		it.get(NULL, CT_RULE_CONDITION_ITEM, &c_name);
		it.get(NULL, CT_RULE_CONDITION_OPTION, &tmp_option);
		c_inst = get_instance_name(c_name, tmp_option);
		c_option.set(NULL, CT_RULE_CONDITION_OPTION, tmp_option);
		c_option_str = c_option.dup_cstr();

		c_record.set(NULL, "rule_id", rid);
		c_record.set(NULL, "name", c_name);
		c_record.set(NULL, "option", (c_option_str)? c_option_str : "");
		c_record.set(NULL, "instance_name", c_inst);
		c_record.set(NULL, "zone", zone);

		ret = db_manager::insert(2, CONDITION_TABLE, c_record, NULL);
		IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Insert conditions to db failed");

		free(c_option_str);
	}

	_D("Add rule%d succeeded", (int)rid);
	return ERR_NONE;
}


int ctx::rule_manager::remove_rule(int rule_id)
{
	// Delete rule from DB
	bool ret;

	std::string query = DELETE_RULE_STATEMENT;
	query += int_to_string(rule_id);
	std::vector<json> record;
	ret = db_manager::execute_sync(query.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Remove rule from db failed");

	return ERR_NONE;
}


int ctx::rule_manager::enable_rule(int rule_id)
{
	// Subscribe event
	bool ret;
	int error;
	std::string id_str = int_to_string(rule_id);

	// Get rule json by rule id;
	std::string q1 = QUERY_RULE_AND_ZONE_BY_RULE_ID;
	q1 += int_to_string(rule_id);
	std::vector<json> rule_record;
	ret = db_manager::execute_sync(q1.c_str(), &rule_record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query rule by rule id failed");
	std::string r1;
	std::string zone;
	rule_record[0].get(NULL, "details", &r1);
	rule_record[0].get(NULL, "zone", &zone);
	ctx::json rule = r1;
	ctx::json event;
	rule.get(NULL, CT_RULE_EVENT, &event);

	// Get event template by rule id
	std::string q2 = QUERY_EVENT_TEMPLATE_BY_RULE_ID;
	q2 += int_to_string(rule_id);
	q2 += ")";
	std::vector<json> etemplate_record;
	ret = db_manager::execute_sync(q2.c_str(), &etemplate_record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query event template by rule id failed");
	std::string r2;
	etemplate_record[0].get(NULL, "j_template", &r2);
	ctx::json etemplate = r2;

	// Query name, instance name & template for conditions of the rule
	std::string q3 = QUERY_NAME_INSTANCE_NAME_AND_TEMPLATE_BY_RULE_ID_STATEMENT;
	q3 += id_str;
	std::vector<json> cond_record;
	ret = db_manager::execute_sync(q3.c_str(), &cond_record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query condition's names, instance names, templates by rule id failed");

	ctx::json inst_names;
	std::vector<json>::iterator vec_end = cond_record.end();
	for (std::vector<json>::iterator vec_pos = cond_record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;

		std::string cname;
		std::string ciname;
		std::string temp;
		elem.get(NULL, "name", &cname);
		elem.get(NULL, "instance_name", &ciname);
		elem.get(NULL, "templates", &temp);
		ctx::json ctemplate = temp;
		ctemplate.set(NULL, "instance_name", ciname);
		// TODO ctemplate (name, instance_name, attributes) now - it is based on form using when templates are inserted into db when rebooting

		// For defrule script generation
		inst_names.set(NULL, cname.c_str(), ciname);

		if (cname.compare(ciname) != 0) {
			if (!clips_h.find_instance(ciname)) {
				std::string makeinst_script = script_generator::generate_makeinstance(&ctemplate);
				error = clips_h.make_instance(makeinst_script);
				IF_FAIL_RETURN_TAG(error == ERR_NONE, ERR_OPERATION_FAILED, _E, "Add condition instance([%s]) failed", ciname.c_str());

				cond_cnt_map[ciname] = 1;
			} else {
				cond_cnt_map[ciname]++;
			}
		}
	}

	// Subscribe event
	std::string ename;
	etemplate.get(NULL, "name", &ename);

	//TODO: set the proper zone
	error = c_monitor.subscribe(rule_id, ename, event, zone.c_str());
	IF_FAIL_RETURN(error == ERR_NONE, ERR_OPERATION_FAILED);

	// Generate defrule script and execute it
	std::string script = script_generator::generate_defrule(id_str, etemplate, rule, &inst_names, zone);
	error = clips_h.define_rule(script);
	IF_FAIL_RETURN_TAG(error == ERR_NONE, ERR_OPERATION_FAILED, _E, "Defrule failed");

	// Update db to set 'enabled'
	std::string q4 = UPDATE_RULE_ENABLED_STATEMENT;
	q4 += id_str;
	std::vector<json> record;
	ret = db_manager::execute_sync(q4.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Update db failed");

	_D(YELLOW("Enable Rule%d succeeded"), rule_id);

	return ERR_NONE;
}

std::string ctx::rule_manager::get_instance_name(std::string name, ctx::json& option)
{
	std::string inst_name = name;

	// Get template for the option
	std::string q = "SELECT j_template FROM context_trigger_template WHERE name = '";
	q += name;
	q += "'";
	std::vector<json> template_record;
	db_manager::execute_sync(q.c_str(), &template_record);

	std::string ct_str;
	template_record[0].get(NULL, "j_template", &ct_str);
	ctx::json j_template = ct_str;

	std::string option_key;
	for (int i = 0; j_template.get_array_elem(NULL, "option", i, &option_key); i++) {
		std::string val_str;
		int val;
		if (option.get(NULL, option_key.c_str(), &val_str)) {
			inst_name += INSTANCE_NAME_DELIMITER;
			inst_name += val_str;
		} else if (option.get(NULL, option_key.c_str(), &val)) {
			inst_name += INSTANCE_NAME_DELIMITER;
			inst_name += int_to_string(val);
		} else {
			inst_name += INSTANCE_NAME_DELIMITER;
		}
	}

	return inst_name;
}

int ctx::rule_manager::disable_rule(int rule_id)
{
	int error;
	bool ret;

	// For event with options
	// Get rule json by rule id;
	std::string q1 = QUERY_RULE_AND_ZONE_BY_RULE_ID;
	q1 += int_to_string(rule_id);
	std::vector<json> rule_record;
	ret = db_manager::execute_sync(q1.c_str(), &rule_record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query rule by rule id failed");
	std::string r1;
	std::string zone;
	rule_record[0].get(NULL, "details", &r1);
	rule_record[0].get(NULL, "zone", &zone);
	ctx::json rule = r1;
	ctx::json event;
	rule.get(NULL, CT_RULE_EVENT, &event);
	std::string ename;
	event.get(NULL, CT_RULE_EVENT_ITEM, &ename);

	// Unsubscribe event
	error = c_monitor.unsubscribe(rule_id, ename, event, zone.c_str());
	IF_FAIL_RETURN(error == ERR_NONE, ERR_OPERATION_FAILED);

	// Undef rule in clips
	std::string id_str = int_to_string(rule_id);
	std::string script = script_generator::generate_undefrule(id_str);
	error = clips_h.route_string_command(script);
	IF_FAIL_RETURN_TAG(error == ERR_NONE, ERR_OPERATION_FAILED, _E, "Undefrule failed");

	// Update db to set 'disabled'
	std::string q2 = UPDATE_RULE_DISABLED_STATEMENT;
	q2 += id_str;
	std::vector<json> record;
	ret = db_manager::execute_sync(q2.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Update db failed");

	// Remove condition instances TODO
	std::string q3 = "SELECT name, instance_name FROM context_trigger_condition WHERE rule_id = ";
	q3 += id_str;
	std::vector<json> name_record;
	ret = db_manager::execute_sync(q3.c_str(), &name_record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query condition's name, instance names by rule id failed");

	std::vector<json>::iterator vec_end = name_record.end();
	for (std::vector<json>::iterator vec_pos = name_record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;

		std::string cname;
		std::string ciname;
		elem.get(NULL, "name", &cname);
		elem.get(NULL, "instance_name", &ciname);

		if (cname.compare(ciname) != 0) {
			cond_cnt_map[ciname]--;

			if (cond_cnt_map[ciname] == 0) {
				error = clips_h.unmake_instance(ciname);
				IF_FAIL_RETURN(error == ERR_NONE, error);

				cond_cnt_map.erase(ciname);
			}
		}
	}

	return ERR_NONE;
}

void ctx::rule_manager::make_condition_option_based_on_event_data(ctx::json& ctemplate, ctx::json& edata, ctx::json* coption)
{
	std::string option_key;
	for (int i = 0; ctemplate.get_array_elem(NULL, "option", i, &option_key); i++) {
		std::string coption_valstr;
		if (coption->get(NULL, option_key.c_str(), &coption_valstr)) {
			if (coption_valstr.find(EVENT_KEY_PREFIX) == 0) {
				std::string event_key = coption_valstr.substr(1, coption_valstr.length() - 1);

				std::string e_valstr;
				int e_val;
				if (edata.get(NULL, event_key.c_str(), &e_valstr)) {
					coption->set(NULL, option_key.c_str(), e_valstr);
				} else if (edata.get(NULL, event_key.c_str(), &e_val)) {
					coption->set(NULL, option_key.c_str(), e_val);
				}
			}
		}
	}
}

void ctx::rule_manager::on_event_received(std::string item, ctx::json option, ctx::json data, std::string zone)
{
	_D(YELLOW("Event(%s(%s) - %s) is invoked."), item.c_str(), option.str().c_str(), data.str().c_str());
	// TODO: Check permission of an event(item), if permission denied, return

	int err;
	bool ret;
	// Set the zone
	ret = clips_h.set_global_variable_string("zone", zone);
	if (!ret) {
		_E("Set clips zone to %s failed", zone.c_str());
		return;
	}

	// Generate event fact script
	std::string q1 = "SELECT j_template FROM context_trigger_template WHERE name = '";
	q1 += item;
	q1 += "'";
	std::vector<json> etemplate_record;
	db_manager::execute_sync(q1.c_str(), &etemplate_record);
	std::string r1;
	etemplate_record[0].get(NULL, "j_template", &r1);
	ctx::json etemplate = r1;

	std::string eventfact_str = script_generator::generate_fact(item, etemplate, option, data);

	// Get Conditions template of invoked event (db query)
	std::string e_inst = get_instance_name(item, option);
	std::string query = QUERY_CONDITION_TEMPLATES_OF_INVOKED_EVENT_STATEMENT;
	query += e_inst;
	query += "' AND zone = '";
	query += zone;
	query += "'))";
	std::vector<json> conds;
	ret = db_manager::execute_sync(query.c_str(), &conds);
	IF_FAIL_VOID_TAG(ret, _E, "Query condition templates of invoked event failed");

	int cond_num = conds.size();
	for (int i = 0; i < cond_num; i++) {
		std::string temp;
		conds[i].get(NULL, "j_template", &temp);
		ctx::json ctemplate = temp;

		std::string cname;
		conds[i].get(NULL, "name", &cname);

		std::string ciname;
		conds[i].get(NULL, "instance_name", &ciname);

		std::string coption_str;
		conds[i].get(NULL, "option", &coption_str);
		ctx::json coption = NULL;
		if (!coption_str.empty()) {
			ctx::json coption_tmp = coption_str;
			coption_tmp.get(NULL, CT_RULE_CONDITION_OPTION, &coption);
		}

		// Check if the condition uses event data key as an option
		if (ciname.find(EVENT_KEY_PREFIX) != std::string::npos) {
			make_condition_option_based_on_event_data(ctemplate, data, &coption);
		}

		// TODO: Check permission of a condition(cname), if permission granted, read condition data. (or, condition data should be empty json)

		//	Get Context Data and Set the proper zone.
		ctx::json condition_data;
		err = c_monitor.read(cname, coption, zone.c_str(), &condition_data);
		if (err != ERR_NONE)
			return;
		_D(YELLOW("Condition(%s(%s) - %s)."), cname.c_str(), coption.str().c_str(), condition_data.str().c_str());

		// Generate ModifyInstance script
		std::string modifyinst_script = script_generator::generate_modifyinstance(ciname, ctemplate, condition_data);

		err = clips_h.route_string_command(modifyinst_script);
		IF_FAIL_VOID_TAG(err == ERR_NONE, _E, "Modify condition instance failed");
	}

	// Add fact and Run environment
	err = clips_h.add_fact(eventfact_str);
	IF_FAIL_VOID_TAG(err == ERR_NONE, _E, "Assert event fact failed");

	err = clips_h.run_environment();
	IF_FAIL_VOID_TAG(err == ERR_NONE, _E, "Run environment failed");

	// Retract event fact
	std::string retract_command = "(retract *)";
	err = clips_h.route_string_command(retract_command);
	IF_FAIL_VOID_TAG(err == ERR_NONE, _E, "Retract event fact failed");
}

static void trigger_action_app_control(ctx::json& action, std::string zone)
{
	int error;
	std::string appctl_str;
	action.get(NULL, CT_RULE_ACTION_APP_CONTROL, &appctl_str);

	char* str = static_cast<char*>(malloc(appctl_str.length()));
	if (str == NULL) {
		_E("Memory allocation failed");
		return;
	}
	appctl_str.copy(str, appctl_str.length(), 0);
	bundle_raw* encoded = reinterpret_cast<unsigned char*>(str);
	bundle* appctl_bundle = bundle_decode(encoded, appctl_str.length());

	app_control_h app = NULL;
	app_control_create(&app);
	app_control_import_from_bundle(app, appctl_bundle);

#if _ZONE_ENABLED_
	char* op;
	app_control_get_operation(app, &op);
	app_control_set_operation(app, APP_SVC_OPERATION_JUMP);
	app_control_add_extra_data(app, APP_SVC_K_JUMP_ZONE_NAME, zone.c_str());
	if (op != NULL) {
		app_control_add_extra_data(app, APP_SVC_K_JUMP_ORIGIN_OPERATION, op);
	}
#endif

	error = app_control_send_launch_request(app, NULL, NULL);
	if (error != APP_CONTROL_ERROR_NONE) {
		_E("Launch request failed(%d)", error);
	} else {
		_D("Launch request succeeded");
	}
	bundle_free(appctl_bundle);
	free(str);
	app_control_destroy(app);
}

static void trigger_action_notification(ctx::json& action, std::string creator, std::string zone)
{
	int error;
	notification_h notification = notification_create(NOTIFICATION_TYPE_NOTI);
	std::string title;
	if (action.get(NULL, CT_RULE_ACTION_NOTI_TITLE, &title)) {
		error = notification_set_text(notification, NOTIFICATION_TEXT_TYPE_TITLE, title.c_str(), NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification title failed(%d)", error);
		}
	}

	std::string content;
	if (action.get(NULL, CT_RULE_ACTION_NOTI_CONTENT, &content)) {
		error = notification_set_text(notification, NOTIFICATION_TEXT_TYPE_CONTENT, content.c_str(), NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification contents failed(%d)", error);
		}
	}

	std::string image_path;
	if (action.get(NULL, CT_RULE_ACTION_NOTI_ICON_PATH, &image_path)) {
		error = notification_set_image(notification, NOTIFICATION_IMAGE_TYPE_ICON, image_path.c_str());
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification icon image failed(%d)", error);
		}
	}

	std::string appctl_str;
	char* str = NULL;
	bundle_raw* encoded = NULL;
	bundle* appctl_bundle = NULL;
	app_control_h app = NULL;
	if (action.get(NULL, CT_RULE_ACTION_APP_CONTROL, &appctl_str)) {
		str = static_cast<char*>(malloc(appctl_str.length()));
		if (str == NULL) {
			_E("Memory allocation failed");
			notification_free(notification);
			return;
		}
		appctl_str.copy(str, appctl_str.length(), 0);
		encoded = reinterpret_cast<unsigned char*>(str);
		appctl_bundle = bundle_decode(encoded, appctl_str.length());

		app_control_create(&app);
		app_control_import_from_bundle(app, appctl_bundle);

		error = notification_set_launch_option(notification, NOTIFICATION_LAUNCH_OPTION_APP_CONTROL, app);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set launch option failed(%d)", error);
		}
	}

	error = notification_set_pkgname(notification, creator.c_str());
	if (error != NOTIFICATION_ERROR_NONE) {
		_E("Set pkgname(%s) failed(%d)", creator.c_str(), error);
	}

	ctx::scope_zone_joiner sz(zone.c_str());

	error = notification_post(notification);
	if (error != NOTIFICATION_ERROR_NONE) {
		_E("Post notification failed(%d)", error);
	} else {
		_D("Post notification succeeded");
	}

	bundle_free(appctl_bundle);
	free(str);
	notification_free(notification);
	if (app) {
		app_control_destroy(app);
	}
}

void ctx::rule_manager::on_rule_triggered(int rule_id)
{
	_D(YELLOW("Rule%d is triggered"), rule_id);

	std::string q = "SELECT details, creator, zone FROM context_trigger_rule WHERE row_id =";
	q += int_to_string(rule_id);

	std::vector<json> record;
	db_manager::execute_sync(q.c_str(), &record);

	std::string details_str;
	record[0].get(NULL, "details", &details_str);
	ctx::json details = details_str;
	ctx::json action;
	details.get(NULL, CT_RULE_ACTION, &action);
	std::string zone;
	record[0].get(NULL, "zone", &zone);

	std::string type;
	if (action.get(NULL, CT_RULE_ACTION_TYPE, &type)) {
		if (type.compare(CT_RULE_ACTION_TYPE_APP_CONTROL) == 0) {
			trigger_action_app_control(action, zone);
		} else if (type.compare(CT_RULE_ACTION_TYPE_NOTIFICATION) == 0) {
			std::string creator;
			record[0].get(NULL, "creator", &creator);
			trigger_action_notification(action, creator, zone);
		}
	}
}

int ctx::rule_manager::check_rule(std::string creator, int rule_id)
{
	// Get creator app id
	std::string q = "SELECT creator FROM context_trigger_rule WHERE row_id =";
	q += int_to_string(rule_id);

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query creator by rule id failed");

	if (record.size() == 0) {
		return ERR_NO_DATA;
	}

	std::string c;
	record[0].get(NULL, "creator", &c);

	if (c.compare(creator) == 0){
		return ERR_NONE;
	}

	return ERR_NO_DATA;
}

bool ctx::rule_manager::is_rule_enabled(int rule_id)
{
	std::string q = "SELECT enabled FROM context_trigger_rule WHERE row_id =";
	q += int_to_string(rule_id);

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Query enabled by rule id failed");

	int enabled;
	record[0].get(NULL, "enabled", &enabled);

	if (enabled == 1)
		return true;
	else
		return false;
}

int ctx::rule_manager::get_rule_by_id(std::string creator, int rule_id, ctx::json* request_result)
{
//	std::string q = "SELECT description, details FROM context_trigger_rule WHERE (creator = '";
	std::string q = "SELECT description FROM context_trigger_rule WHERE (creator = '";
	q += creator;
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
//	std::string details;
	record[0].get(NULL, "description", &description);
//	record[0].get(NULL, "details", &details);

	(*request_result).set(NULL, CT_RULE_ID, rule_id);
	(*request_result).set(NULL, CT_RULE_DESCRIPTION, description);
//	(*request_result).set(NULL, CT_RULE_DETAILS, details);

	return ERR_NONE;
}

int ctx::rule_manager::get_rule_ids(std::string creator, ctx::json* request_result)
{
	(*request_result) = "{ \"" CT_RULE_ARRAY_ENABLED "\" : [ ] , \"" CT_RULE_ARRAY_DISABLED "\" : [ ] }";

	std::string q = "SELECT row_id, enabled FROM context_trigger_rule WHERE (creator = '";
	q += creator;
	q += "')";

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query rules failed");

	std::vector<json>::iterator vec_end = record.end();
	for (std::vector<json>::iterator vec_pos = record.begin(); vec_pos != vec_end; ++vec_pos) {
		ctx::json elem = *vec_pos;
		std::string id;
		int enabled;

		elem.get(NULL, "row_id", &id);
		elem.get(NULL, "enabled", &enabled);

		if (enabled == 1) {
			(*request_result).array_append(NULL, CT_RULE_ARRAY_ENABLED, string_to_int(id));
		} else if (enabled == 0) {
			(*request_result).array_append(NULL, CT_RULE_ARRAY_DISABLED, string_to_int(id));
		}
	}

	return ERR_NONE;
}
