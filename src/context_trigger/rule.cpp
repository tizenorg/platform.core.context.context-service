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

#define CONTEXT_RULE_EVENT "event"
#define CONTEXT_RULE_CONDITION "condition"
#define CONTEXT_RULE_NAME "name"
#define CONTEXT_RULE_OPTION "option"
#define CONTEXT_RULE_DATA "data"

ctx::trigger_rule::trigger_rule()
{
}

ctx::trigger_rule::trigger_rule(int i, ctx::json& d, const char* cr, context_monitor* cm)
	: ctx_monitor(cm)
	, id(i)
	, creator(cr)
{
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
	// Subscribe event
	int error = ctx_monitor->subscribe(id, event->name, event->option, this);
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to start rule%d", id);

	return error;
}

int ctx::trigger_rule::stop(void)
{
	// Unsubscribe event
	int error = ctx_monitor->unsubscribe(id, event->name, event->option, this);
	IF_FAIL_RETURN_TAG(error == ERR_NONE, error, _E, "Failed to stop rule%d", id);

	return error;
}

void ctx::trigger_rule::on_event_received(std::string name, ctx::json option, ctx::json data)
{
	clear_result();
	_D("Rule%d received event data", id);

	// Set event data
	result.set(CONTEXT_RULE_EVENT, CONTEXT_RULE_NAME, name);
	result.set(CONTEXT_RULE_EVENT, CONTEXT_RULE_OPTION, option);
	result.set(CONTEXT_RULE_EVENT, CONTEXT_RULE_DATA, data);

	if (condition.size() == 0) {
		on_context_data_prepared(data);
		return;
	}

	for (std::list<context_item_t>::iterator it = condition.begin(); it != condition.end(); ++it) {
		// TODO send read request for each condition
	}

	// TODO timer set
}

void ctx::trigger_rule::on_condition_received(std::string name, ctx::json option, ctx::json data)
{
	// Set condition data
	ctx::json item;
	item.set(NULL, CONTEXT_RULE_NAME, name);
	item.set(NULL, CONTEXT_RULE_OPTION, option);
	item.set(NULL, CONTEXT_RULE_DATA, data);
	result.array_append(NULL, CONTEXT_RULE_CONDITION, item);

	if (result.array_get_size(NULL, CONTEXT_RULE_CONDITION) == (int) condition.size()) {
		on_context_data_prepared(data);
	}
}

void ctx::trigger_rule::clear_result()
{
	result = json();
	// TODO timer cancel
}

void ctx::trigger_rule::on_context_data_prepared(ctx::json& data)
{
	if (ctx::rule_evaluator::evaluate_rule(statement, data)) {
		ctx::action_manager::trigger_action(action, creator);
	}

	clear_result();
}
