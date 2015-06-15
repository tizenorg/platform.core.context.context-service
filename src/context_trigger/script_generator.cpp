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
#include <set>
#include <glib.h>
#include <string>
#include <json.h>
#include <types_internal.h>
#include <context_trigger_types_internal.h>
#include "script_generator.h"
#include "timer_types.h"

#define EVENT_WEEKDAY "(or (eq ?DayOfWeek \"Mon\") (eq ?DayOfWeek \"Tue\") (eq ?DayOfWeek \"Wed\") (eq ?DayOfWeek \"Thu\") (eq ?DayOfWeek \"Fri\"))"
#define EVENT_WEEKEND "(or (eq ?DayOfWeek \"Sat\") (eq ?DayOfWeek \"Sun\"))"
#define CONDITION_WEEKDAY "(or (eq (send [%s] get-DayOfWeek) \"Mon\") (eq (send [%s] get-DayOfWeek) \"Tue\") (eq (send [%s] get-DayOfWeek) \"Wed\") (eq (send [%s] get-DayOfWeek) \"Thu\") (eq (send [%s] get-DayOfWeek) \"Fri\"))"
#define CONDITION_WEEKEND "(or (eq (send [%s] get-DayOfWeek) \"Sat\") (eq (send [%s] get-DayOfWeek) \"Sun\"))"

static std::string generate_initial_fact(ctx::json event_template, ctx::json option);
static std::string generate_event_data(ctx::json event);
static std::string generate_condition_data(std::string rule_id, ctx::json conditions, std::string rule_op, ctx::json* inst_names);

static std::string int_to_string(int i)
{
	std::ostringstream convert;
	convert << i;
	std::string str = convert.str();
	return str;
}

static std::string convert_condition_weekday_weekend(std::string inst_name, std::string day)
{
	std::string buf = (day.compare(TIMER_WEEKDAY) == 0)? CONDITION_WEEKDAY : CONDITION_WEEKEND;

	size_t pos = 0;
	while((pos = buf.find("%s", pos)) != std::string::npos) {
		buf.replace(pos, 2, inst_name);
		pos += inst_name.length();
	}

	return buf;
}

std::string ctx::script_generator::generate_deftemplate(ctx::json* item)
{
	std::string script;

	std::string name;
	item->get(NULL, "name", &name);

	std::set<std::string> slot;

	std::string opt_name;
	for (int i = 0; item->get_array_elem(NULL, "option", i, &opt_name); i++) {
		slot.insert(opt_name);
	}

	std::string attr_name;
	for (int i = 0; item->get_array_elem(NULL, "attributes", i, &attr_name); i++) {
		slot.insert(attr_name);
	}

	//template name is "itemname"
	script = "(deftemplate ";
	script += name;
	script += " ";

	for (std::set<std::string>::iterator it = slot.begin(); it != slot.end(); ++it){
		script += "(slot ";
		script += *it;
		script += " (default null))";
	}
	script += ")";

//	_D("Deftemplate script is generated: %s", script.c_str());
	script += "\n";

	return script;
}

std::string ctx::script_generator::generate_defclass(ctx::json* item)
{
	std::string script;

	std::string name;
	item->get(NULL, "name", &name);

	//class name is "C.itemname"
	script = "(defclass C.";
	script += name;
	script += " (is-a USER) (role concrete) ";

	std::string attr_name;
	for (int i = 0; item->get_array_elem(NULL, "attributes", i, &attr_name); i++) {
		script += "(slot ";
		script += attr_name;
		script += " (create-accessor read-write))";
	}
	script += ")";

//	_D("Defclass script is generated: %s", script.c_str());
	script += "\n";

	return script;
}

std::string ctx::script_generator::generate_makeinstance(ctx::json* item)
{
	std::string script;

	std::string name;
	item->get(NULL, "name", &name);

	std::string instance_name;
	if (!item->get(NULL, "instance_name", &instance_name)) {
		instance_name = name;
	}

	//instance name is "[itemname]"
	script = "([";
	script += instance_name;
	script += "] of C.";
	script += name;

	std::string attr_name;
	for (int i = 0; item->get_array_elem(NULL, "attributes", i, &attr_name); i++) {
		script += " (";
		script += attr_name;
		script += " 0)";
	}
	script += ")";

//	_D("Makeinstance script is generated: %s", script.c_str());
	script += "\n";

	return script;
}

std::string ctx::script_generator::generate_undefrule(std::string rule_id)
{
	std::string script;
	script = "(undefrule rule";
	script += rule_id;
	script += ")";

	return script;
}

std::string ctx::script_generator::generate_defrule(std::string rule_id, ctx::json event_template, ctx::json rule, ctx::json* inst_names, std::string zone)
{
	std::string script;
	ctx::json option = NULL;
	rule.get(CT_RULE_EVENT, CT_RULE_EVENT_OPTION, &option);

	script = "(defrule rule";
	script += rule_id;
	script += " ";

	script += generate_initial_fact(event_template, option);
	script += " => ";

	int eventdata_num = rule.array_get_size(CT_RULE_EVENT, CT_RULE_DATA_ARR);
	int condition_num = rule.array_get_size(NULL, CT_RULE_CONDITION);

	if (condition_num > 0) {
		// case1: condition exists
		script += "(if (and (eq ?*zone* \"";
		script += zone;
		script += "\") ";

		if (eventdata_num > 0) {
			ctx::json event;
			rule.get(NULL, CT_RULE_EVENT, &event);
			script += generate_event_data(event);
		}

		// condition
		ctx::json conditions;
		rule.get(NULL, CT_RULE_CONDITION, &conditions);
		std::string rule_op;
		rule.get(NULL, CT_RULE_OPERATOR, &rule_op);
		script += generate_condition_data(rule_id, conditions, rule_op, inst_names);

		script += ")";
		script += " then ";
		script += " (execute_action rule";
		script += rule_id;
		script += "))";
	} else if (eventdata_num > 0) {
		// case2: no conditions, but event data
		ctx::json event;
		rule.get(NULL, CT_RULE_EVENT, &event);

		script += "(if (and (eq ?*zone* \"";
		script += zone;
		script += "\") ";
		script += generate_event_data(event);
		script += ") then ";
		script += " (execute_action rule";
		script += rule_id;
		script += "))";
	} else {
		// case3: only action
		script += "if (eq ?*zone* \"";
		script += zone;
		script += "\") then (execute_action rule";
		script += rule_id;
		script += ")";
	}

	script += ")";
	_D("Defrule script generated: %s", script.c_str());
	return script;
}

std::string generate_initial_fact(ctx::json event_template, ctx::json option)
{
	std::string script = "(";
	std::string e_name;
	event_template.get(NULL, "name", &e_name);

	script += e_name;
	script += " ";

	// options
	std::string opt_key;
	for (int i = 0; event_template.get_array_elem(NULL, "option", i, &opt_key); i++) {
		script += "(";
		script += opt_key;
		script += " ";

		std::string val_str;
		int val;
		if (option.get(NULL, opt_key.c_str(), &val_str)) {
			script += val_str;
		} else if (option.get(NULL, opt_key.c_str(), &val)) {
			script += int_to_string(val);
		} else {
			script += "?";
			script += opt_key;
		}

		script += ")";
	}

	// attributes
	std::string attr;
	for (int i = 0; event_template.get_array_elem(NULL, "attributes", i, &attr); i++) {
		script += "(";
		script += attr;
		script += " ?";
		script += attr;
		script += ")";
	}
	script += ")";

//	_D("Initial event fact is %s", script.c_str());
	return script;
}

std::string generate_event_data(ctx::json event)
{
	std::string ename;
	event.get(NULL, CT_RULE_EVENT_ITEM, &ename);

	std::string script;
	int edata_count = event.array_get_size(NULL, CT_RULE_DATA_ARR);

	if (edata_count > 1) {
		std::string item_op;
		event.get(NULL, CT_RULE_EVENT_OPERATOR, &item_op);
		script += "(";
		script += item_op;
		script += " ";
	}

	ctx::json edata;
	for (int i = 0; event.get_array_elem(NULL, CT_RULE_DATA_ARR, i, &edata); i++) {
		std::string key_name;
		if (ename.compare(TIMER_EVENT_SUBJECT) == 0) {
			edata.get(NULL, CT_RULE_DATA_KEY, &key_name);
		}

		int valuecount = edata.array_get_size(NULL, CT_RULE_DATA_VALUE_ARR);
		int opcount = edata.array_get_size(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR);
		std::string key;
		edata.get(NULL, CT_RULE_DATA_KEY, &key);

		if (valuecount != opcount) {
			_E("Invalid event data. (Value count:%d, Operator count: %d)", valuecount, opcount);
			return "";
		}

		if (valuecount > 1) {
			script += "(";

			std::string key_op;
			edata.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &key_op);
			script += key_op;
			script += " ";
		}

		for (int j = 0; j < valuecount; j++){
			std::string val_op;
			std::string val;
			edata.get_array_elem(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, j, &val_op);
			edata.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, j, &val);

			if (key_name.compare(TIMER_RESPONSE_KEY_DAY_OF_WEEK) == 0) {
				if (val.compare("\"" TIMER_WEEKDAY "\"") == 0) {
					script += (val_op.compare("eq") == 0)? EVENT_WEEKDAY : EVENT_WEEKEND;
					continue;
				} else if (val.compare("\"" TIMER_WEEKEND "\"") == 0) {
					script += (val_op.compare("eq") == 0)? EVENT_WEEKEND : EVENT_WEEKDAY;
					continue;
				}
			}

			script += "(";
			script += val_op;
			script += " ?";
			script += key;
			script += " ";
			script += val;
			script += ")";
		}

		if (valuecount > 1) {
			script += ")";
		}
	}

	if (edata_count > 1) {
		script += ")";
	}

	return script;
}

std::string generate_condition_data(std::string rule_id, ctx::json conditions, std::string rule_op, ctx::json* inst_names)
{
	std::string script;

	ctx::json conds;
	conds.set(NULL, CT_RULE_CONDITION, conditions);

	int conds_count = conds.array_get_size(NULL, CT_RULE_CONDITION);
	if (conds_count > 1) {
		script += "(";
		script += rule_op;	// operator between each condition item
		script += " ";
	}

	ctx::json it;
	for (int i = 0; conds.get_array_elem(NULL, CT_RULE_CONDITION, i, &it); i++) {
		std::string cond_name;
		it.get(NULL, CT_RULE_CONDITION_ITEM, &cond_name);

		std::string inst_name;
		(inst_names)->get(NULL, cond_name.c_str(), &inst_name);

		int data_count = it.array_get_size(NULL, CT_RULE_DATA_ARR);

		if (data_count > 1) {
			std::string item_op;
			it.get(NULL, CT_RULE_CONDITION_OPERATOR, &item_op);
			script += "(";
			script += item_op; // operator between data keys of a condition item
			script += " ";
		}

		ctx::json data;
		for (int j = 0; it.get_array_elem(NULL, CT_RULE_DATA_ARR, j, &data); j++) {
			std::string key_name;
			data.get(NULL, CT_RULE_DATA_KEY, &key_name);
			int dval_count = data.array_get_size(NULL, CT_RULE_DATA_VALUE_ARR);
			int dvalop_count = data.array_get_size(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR);

			if (dval_count != dvalop_count) {
				_E("Invalid condition data. (Data value count %d, Data value operator count %d)", dval_count, dvalop_count);
			}

			if (dval_count > 1) {
				std::string dkey_op;
				data.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &dkey_op);

				script += "(";
				script += dkey_op; // operator between comparison data values of a condition data key
				script += " " ;
			}

			for (int k = 0; k < dval_count; k++) {
				std::string dval_op;
				std::string dval;
				data.get_array_elem(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, k, &dval_op);
				data.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, k, &dval);

				if (inst_name.compare(TIMER_CONDITION_SUBJECT) == 0 && key_name.compare(TIMER_RESPONSE_KEY_DAY_OF_WEEK) == 0) {
					if (dval.compare("\"" TIMER_WEEKDAY "\"") == 0) {
						script += convert_condition_weekday_weekend(inst_name, (dval_op.compare("eq") == 0)? TIMER_WEEKDAY : TIMER_WEEKEND);
						continue;
					} else if (dval.compare("\"" TIMER_WEEKEND "\"") == 0) {
						script += convert_condition_weekday_weekend(inst_name, (dval_op.compare("eq") == 0)? TIMER_WEEKEND : TIMER_WEEKDAY);
						continue;
					}
				}

				script += "(";
				script += dval_op;
				script += " (send [";
				script += inst_name;
				script += "] get-";
				script += key_name;
				script += ") ";
				script += dval;
				script += ")";
			}

			if (dval_count > 1) {
				script += ")";
			}
		}

		if (data_count > 1 ) {
			script += ")";
		}
	}

	if (conds_count > 1) {
		script += ")";
	}

	return script;
}

std::string ctx::script_generator::generate_fact(std::string item_name, ctx::json event_template, ctx::json option, ctx::json data)
{
	// Generate Fact script for invoked event
	std::string script = "(";
	script += item_name;
	script += " ";

	std::string opt_key;
	std::string key;
	std::string val_str;
	int value;

	for (int i = 0; event_template.get_array_elem(NULL, "option", i, &opt_key); i++) {
		script += "(";
		script += opt_key;
		script += " ";
		if (option.get(NULL, opt_key.c_str(), &val_str)) {	// string type data
			script += val_str;
		} else if (option.get(NULL, opt_key.c_str(), &value)) {	// integer type data
			script += int_to_string(value);
		} else {
			script += "nil";
		}
		script += ")";
	}

	for (int i = 0; event_template.get_array_elem(NULL, "attributes", i, &key); i++) {
		script += "(";
		script += key;
		script += " ";
		if (data.get(NULL, key.c_str(), &val_str)) {	// string type data
			script += "\"" + val_str + "\"";
		} else if (data.get(NULL, key.c_str(), &value)) {	// integer type data
			script += int_to_string(value);
		} else {
			script += "nil";
		}
		script += ")";
	}
	script += ")";

	return script;
}

std::string ctx::script_generator::generate_modifyinstance(std::string instance_name, ctx::json condition_template, ctx::json data)
{
	std::string script = "(modify-instance [";
	script += instance_name;
	script += "] ";

	std::string key;
	std::string val_str;
	int value;
	for (int i = 0; condition_template.get_array_elem(NULL, "attributes", i, &key); i++) {
		if (data.get(NULL, key.c_str(), &val_str)) {	// string type data
			script += "(" + key + " \"" + val_str + "\")";
		} else 	if (data.get(NULL, key.c_str(), &value)) {	// integer type data
			script += "(" + key + " " + int_to_string(value) + ")";
		}
		// else continue;
	}
	script += ")";

	return script;
}
