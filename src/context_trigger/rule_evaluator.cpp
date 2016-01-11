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

#include <json.h>
#include <types_internal.h>
#include <context_trigger_types_internal.h>
#include "rule_evaluator.h"
#include "context_fact_types.h"

#define AND_STRING "and"
#define OR_STRING "or"
#define EVENT_REFERENCE "?"
#define OPERATOR_EQ "=="
#define OPERATOR_NEQ "!="
#define OPERATOR_LEQ "<="
#define OPERATOR_GEQ ">="
#define OPERATOR_LT "<"
#define OPERATOR_GT ">"

ctx::rule_evaluator::rule_evaluator()
{
}

bool ctx::rule_evaluator::compare_string(std::string operation, std::string rule_var, std::string fact_var)
{
	if (operation == OPERATOR_EQ) {
		return (rule_var == fact_var);
	} else if (operation == OPERATOR_NEQ) {
		return (rule_var != fact_var);
	} else {
		_E("Operator %s not supported", operation.c_str());
		return false;
	}
}

bool ctx::rule_evaluator::compare_int(std::string operation, int rule_var, int fact_var)
{
	if (operation == OPERATOR_EQ) {
		return (rule_var == fact_var);
	} else if (operation == OPERATOR_NEQ) {
		return (rule_var != fact_var);
	} else if (operation == OPERATOR_LEQ) {
		return (rule_var <= fact_var);
	} else if (operation == OPERATOR_GEQ) {
		return (rule_var >= fact_var);
	} else if (operation == OPERATOR_LT) {
		return (rule_var < fact_var);
	} else if (operation == OPERATOR_GT) {
		return (rule_var > fact_var);
	} else {
		_E("Operator %s not supported", operation.c_str());
		return false;
	}
}

bool ctx::rule_evaluator::replace_data_references(ctx::json& rule_data_arr, ctx::json event_fact_data)
{
	std::string arr_string;
	std::string event_reference_string;
	int event_reference_int;

	for (int i = 0; i < rule_data_arr.array_get_size(NULL, CT_RULE_DATA_VALUE_ARR); i++) {
		if (!rule_data_arr.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, i, &arr_string)) {
			continue;
		}
		if (arr_string.substr(0, 1) != EVENT_REFERENCE) {
			continue;
		}

		std::string event_key = arr_string.substr(1, arr_string.length() - 1);
		if (event_fact_data.get(NULL, event_key.c_str(), &event_reference_string)) {
			rule_data_arr.array_set_at(NULL, CT_RULE_DATA_VALUE_ARR, i, event_reference_string);
		} else if (event_fact_data.get(NULL, event_key.c_str(), &event_reference_int)) {
			rule_data_arr.array_set_at(NULL, CT_RULE_DATA_VALUE_ARR, i, event_reference_int);
		} else {
			_W("Option %s not found in event_data", event_key.c_str());
		}
	}

	return true;
}

bool ctx::rule_evaluator::replace_option_references(ctx::json& rule_option, ctx::json event_fact_data)
{
	std::list<std::string> key_list;
	rule_option.get_keys(&key_list);

	for (std::list<std::string>::iterator it = key_list.begin(); it != key_list.end(); ++it) {
		std::string option_key = *it;
		std::string rule_key_value_str;

		if (!rule_option.get(NULL, (*it).c_str(), &rule_key_value_str)) {
			continue;
		}
		if (!(rule_key_value_str.substr(0, 1) == EVENT_REFERENCE)) {
			continue;
		}

		int rule_event_key_value_int;
		std::string rule_event_key_value_str;
		std::string event_key = rule_key_value_str.substr(1, rule_key_value_str.length() - 1);
		if (event_fact_data.get(NULL, event_key.c_str(), &rule_event_key_value_str)) {
			rule_option.set(NULL, (*it).c_str(), rule_event_key_value_str);
		} else if (event_fact_data.get(NULL, event_key.c_str(), &rule_event_key_value_int)) {
			rule_option.set(NULL, (*it).c_str(), rule_event_key_value_int);
		} else {
			_W("Option %s not found in event_data", event_key.c_str());
			return false;
		}
	}

	return true;
}

bool ctx::rule_evaluator::replace_event_references(ctx::json& rule, ctx::json& fact)
{
	ctx::json event_fact_data;
	if (!fact.get(CONTEXT_FACT_EVENT, CONTEXT_FACT_DATA, &event_fact_data)) {
		_E("No event data found, error");
		return false;
	}

	ctx::json rule_condition;
	for (int i = 0; rule.get_array_elem(NULL, CT_RULE_CONDITION, i, &rule_condition); i++) {
		ctx::json rule_data_arr;
		for (int j = 0; rule_condition.get_array_elem(NULL, CT_RULE_DATA_ARR, j, &rule_data_arr); j++) {
			replace_data_references(rule_data_arr, event_fact_data);
		}

		ctx::json rule_option;
		if (rule_condition.get(NULL, CT_RULE_CONDITION_OPTION, &rule_option)) {
			replace_option_references(rule_option, event_fact_data);
		}
	}

	return true;
}

bool ctx::rule_evaluator::evaluate_data_string(ctx::json& rule_data_arr, std::string fact_value_str)
{
	std::string operate_internal;
	rule_data_arr.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &operate_internal);
	bool is_and = false;
	if (AND_STRING == operate_internal) {
		is_and = true;
	}

	std::string operator_str;
	for (int j = 0; rule_data_arr.get_array_elem(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, j, &operator_str); j++) {
		bool data_arr_vale_match;
		std::string get_str_val;
		rule_data_arr.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, j, &get_str_val);
		data_arr_vale_match = compare_string(operator_str, fact_value_str, get_str_val);

		if (is_and && !data_arr_vale_match) {
			return false;
		} else if (!is_and && data_arr_vale_match) {
			return true;
		}
	}
	return is_and;
}

bool ctx::rule_evaluator::evaluate_data_int(ctx::json& rule_data_arr, int fact_value_int)
{
	std::string operate_internal;
	rule_data_arr.get(NULL, CT_RULE_DATA_KEY_OPERATOR, &operate_internal);
	bool is_and = false;
	if (AND_STRING == operate_internal) {
		is_and = true;
	}

	std::string operator_str;
	for (int j = 0; rule_data_arr.get_array_elem(NULL, CT_RULE_DATA_VALUE_OPERATOR_ARR, j, &operator_str); j++) {
		bool data_arr_vale_match;
		int get_int_val;
		if (!rule_data_arr.get_array_elem(NULL, CT_RULE_DATA_VALUE_ARR, j, &get_int_val)) {
			data_arr_vale_match = false;
		}
		data_arr_vale_match = compare_int(operator_str, fact_value_int, get_int_val);

		if (is_and && !data_arr_vale_match) {
			return false;
		} else if (!is_and && data_arr_vale_match) {
			return true;
		}
	}
	return is_and;
}

bool ctx::rule_evaluator::evaluate_item(ctx::json& rule_item, ctx::json& fact_item)
{
	ctx::json rule_data_arr;
	if (rule_item.array_get_size(NULL, CT_RULE_DATA_ARR) == 0) {
		return true;
	}

	std::string operate;
	bool is_and = false;
	rule_item.get(NULL, CT_RULE_CONDITION_OPERATOR, &operate);
	if (AND_STRING == operate) {
		is_and = true;
	}

	for (int i = 0; rule_item.get_array_elem(NULL, CT_RULE_DATA_ARR, i, &rule_data_arr); i++) {
		std::string data_key;
		rule_data_arr.get(NULL, CT_RULE_DATA_KEY, &data_key);
		std::string fact_value_str;
		int fact_value_int;

		bool rule_data_arr_true;
		if (fact_item.get(CONTEXT_FACT_DATA, data_key.c_str(), &fact_value_str)) {
			rule_data_arr_true = evaluate_data_string(rule_data_arr, fact_value_str);
		} else if (fact_item.get(CONTEXT_FACT_DATA, data_key.c_str(), &fact_value_int)) {
			rule_data_arr_true = evaluate_data_int(rule_data_arr, fact_value_int);
		} else {
			_W("Could not get value corresponding to data key %s", data_key.c_str());
			rule_data_arr_true = false;
		}

		if (is_and && !rule_data_arr_true) {
			return false;
		} else if (!is_and && rule_data_arr_true) {
			return true;
		}
	}
	return is_and;
}

bool ctx::rule_evaluator::check_rule_event(ctx::json& rule, ctx::json& fact)
{
	ctx::json fact_item;
	fact.get(NULL, CONTEXT_FACT_EVENT, &fact_item);
	ctx::json rule_item;
	rule.get(NULL, CT_RULE_EVENT, &rule_item);
	return evaluate_item(rule_item, fact_item);
}

ctx::json ctx::rule_evaluator::find_condition_fact(ctx::json& rule_condition, ctx::json& fact)
{
	ctx::json fact_condition;
	std::string item_name_rule_condition;
	rule_condition.get(NULL, CT_RULE_CONDITION_ITEM, &item_name_rule_condition);

	std::string item_name_fact_condition;
	for (int i = 0; fact.get_array_elem(NULL, CONTEXT_FACT_CONDITION, i, &fact_condition); i++) {
		fact_condition.get(NULL, CONTEXT_FACT_NAME, &item_name_fact_condition);
		if (item_name_fact_condition != item_name_rule_condition) {
			continue;
		}

		ctx::json rule_condition_option;
		ctx::json fact_condition_option;
		rule_condition.get(NULL, CT_RULE_CONDITION_OPTION, &rule_condition_option);
		fact_condition.get(NULL, CONTEXT_FACT_OPTION, &fact_condition_option);
		if (fact_condition_option == rule_condition_option) {
			return fact_condition;
		}
	}

	_W(YELLOW("find_condition failed for condition"));
	return EMPTY_JSON_OBJECT;
}

bool ctx::rule_evaluator::check_rule_condition(ctx::json& rule, ctx::json& fact)
{
	ctx::json rule_condition;
	ctx::json fact_condition;

	std::string operate;
	rule.get(NULL, CT_RULE_OPERATOR, &operate);
	bool is_and = false;
	if (operate == AND_STRING) {
		is_and = true;
	}

	for (int i = 0; rule.get_array_elem(NULL, CT_RULE_CONDITION, i, &rule_condition); i++) {
		fact_condition = find_condition_fact(rule_condition, fact);
		bool condition_satisfied;
		if (fact_condition == EMPTY_JSON_OBJECT) {
			condition_satisfied = false;
		} else {
			condition_satisfied = evaluate_item(rule_condition, fact_condition);
		}

		if (is_and && !condition_satisfied) {
			return false;
		} else if (!is_and && condition_satisfied) {
			return true;
		}
	}

	return is_and;
}

bool ctx::rule_evaluator::evaluate_rule(ctx::json rule, ctx::json fact)
{
	_D("Rule is %s ", rule.str().c_str());
	_D("fact is %s ", fact.str().c_str());
	rule_evaluator eval;
	bool ret;
	ctx::json temp_json;
	if (fact.get(NULL, CT_RULE_CONDITION, &temp_json)) {
		ctx::json rule_copy(rule.str());
		if (!eval.replace_event_references(rule_copy, fact)) {
			_W("Replace failed");
		}
		ret = eval.check_rule_condition(rule_copy, fact);
		_D("Checking condition %s", ret ? "true" : "false");
	} else {
		ret = eval.check_rule_event(rule, fact);
		_D("Checking event %s", ret ? "true" : "false");
	}
	return ret;
}