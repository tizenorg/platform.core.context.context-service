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

#include <context_trigger_types_internal.h>
#include "rule.h"
#include "action_manager.h"
#include "rule_evaluator.h"
#include "context_monitor.h"
#include "timer_types.h"
#include "context_fact_types.h"
#include "rule_manager.h"

ctx::rule_manager *ctx::trigger_rule::rule_mgr = NULL;

ctx::trigger_rule::trigger_rule()
{
}

ctx::trigger_rule::trigger_rule(int i, ctx::json& d, const char* cr, context_monitor* cm, rule_manager* rm)
	: result(EMPTY_JSON_OBJECT)
	, ctx_monitor(cm)
	, id(i)
	, creator(cr)
{
	// Rule manager
	if (!rule_mgr) {
		rule_mgr = rm;
	}

	// Statement
	statement = d.str();

	// Event
	ctx::json e;
	d.get(NULL, CT_RULE_EVENT, &e);
	event = new(std::nothrow) context_item_s(e);

	// Condition
	int cond_num = d.array_get_size(NULL, CT_RULE_CONDITION);
	for (int j = 0; j < cond_num; j++) {
		ctx::json c;
		d.get_array_elem(NULL, CT_RULE_CONDITION, j, &c);
		condition.push_back(new(std::nothrow) context_item_s(c));
	}

	// Action
	ctx::json a;
	d.get(NULL, CT_RULE_ACTION, &a);
	action = a.str();
}

ctx::trigger_rule::~trigger_rule()
{
	// Release resources
	delete event;
	for (std::list<context_item_t>::iterator it = condition.begin(); it != condition.end(); ++it) {
		delete *it;
	}
}

int ctx::trigger_rule::start(void)
{
	ctx::json time_option = EMPTY_JSON_OBJECT;
	if (event->name.compare(TIMER_EVENT_SUBJECT) == 0) {
		statement.get(NULL, CT_RULE_EVENT, &time_option);
	}

	// Subscribe event
	int error = ctx_monitor->subscribe(id, event->name, (time_option == EMPTY_JSON_OBJECT)? event->option : time_option, this);
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to start rule%d", id);

	return error;
}

int ctx::trigger_rule::stop(void)
{
	ctx::json time_option = EMPTY_JSON_OBJECT;
	if (event->name.compare(TIMER_EVENT_SUBJECT) == 0) {
		statement.get(NULL, CT_RULE_EVENT, &time_option);
	}

	// Unsubscribe event
	int error = ctx_monitor->unsubscribe(id, event->name, (time_option == EMPTY_JSON_OBJECT)? event->option : time_option, this);
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to stop rule%d", id);

	return error;
}

bool ctx::trigger_rule::set_condition_option_based_on_event(ctx::json& option)
{
	// Set condition option if it references event data
	std::list<std::string> option_keys;
	option.get_keys(&option_keys);

	for (std::list<std::string>::iterator it = option_keys.begin(); it != option_keys.end(); ++it) {
		std::string opt_key = (*it);

		std::string opt_val;
		if (option.get(NULL, opt_key.c_str(), &opt_val)) {
			if (opt_val.find("?") != 0) {
				continue;
			}

			std::string event_key = opt_val.substr(1, opt_val.length() - 1);

			std::string new_str;
			int new_val;
			if (result.get(CONTEXT_FACT_EVENT "." CONTEXT_FACT_DATA, event_key.c_str(), &new_str)) {
				option.set(NULL, opt_key.c_str(), new_str);
			} else if (result.get(CONTEXT_FACT_EVENT "." CONTEXT_FACT_DATA, event_key.c_str(), &new_val)) {
				option.set(NULL, opt_key.c_str(), new_val);
			} else {
				_W("Failed to find '%s' in event data", event_key.c_str());
				return false;
			}
		}
	}

	return true;
}

void ctx::trigger_rule::on_event_received(std::string name, ctx::json option, ctx::json data)
{
	if (result != EMPTY_JSON_OBJECT) {
		clear_result();
	}

	// Check if creator is uninstalled
	if (ctx::rule_manager::is_uninstalled_package(creator)) {
		_D("Creator(%s) of rule%d is uninstalled.", creator.c_str(), id);
		g_idle_add(handle_uninstalled_rule, &id);
		return;
	}

	_D("Rule%d received event data", id);

	// Set event data
	result.set(CONTEXT_FACT_EVENT, CONTEXT_FACT_NAME, name);
	result.set(CONTEXT_FACT_EVENT, CONTEXT_FACT_OPTION, option);
	result.set(CONTEXT_FACT_EVENT, CONTEXT_FACT_DATA, data);

	if (condition.size() == 0) {
		on_context_data_prepared();
		return;
	}

	// TODO check if event matched first


	// Request read conditions
	for (std::list<context_item_t>::iterator it = condition.begin(); it != condition.end(); ++it) {
		ctx::json cond_option = (*it)->option.str();
		if (!set_condition_option_based_on_event(cond_option)) { // cond_option should be copy of original option.
			clear_result();
			return;
		}

		int error = ctx_monitor->read((*it)->name.c_str(), cond_option, this);
		IF_FAIL_VOID_TAG(error == ERR_NONE, _E, "Failed to read condition");
	}

	// TODO timer set
}

void ctx::trigger_rule::on_condition_received(std::string name, ctx::json option, ctx::json data)
{
	_D("Rule%d received condition data", id);

	// Set condition data
	ctx::json item;
	item.set(NULL, CONTEXT_FACT_NAME, name);
	item.set(NULL, CONTEXT_FACT_OPTION, option);
	item.set(NULL, CONTEXT_FACT_DATA, data);
	result.array_append(NULL, CONTEXT_FACT_CONDITION, item);

	if (result.array_get_size(NULL, CONTEXT_FACT_CONDITION) == (int) condition.size()) {
		on_context_data_prepared();
	}
}

void ctx::trigger_rule::clear_result()
{
	result = EMPTY_JSON_OBJECT;
	// TODO timer cancel
}

void ctx::trigger_rule::on_context_data_prepared(void)
{
	if (ctx::rule_evaluator::evaluate_rule(statement, result)) {
		ctx::action_manager::trigger_action(action, creator);
	}

	clear_result();
}

gboolean ctx::trigger_rule::handle_uninstalled_rule(gpointer data)
{
	int* rule_id = static_cast<int*>(data);
	rule_mgr->disable_rule(*rule_id);
	rule_mgr->remove_rule(*rule_id);

	return FALSE;
}
