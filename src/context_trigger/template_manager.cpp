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
#include <types_internal.h>
#include <context_trigger_types_internal.h>
#include <db_mgr.h>
#include "../context_mgr_impl.h"
#include "rule_manager.h"
#include "template_manager.h"

static std::string int_to_string(int i)
{
	std::ostringstream convert;
	convert << i;
	std::string str = convert.str();
	return str;
}

ctx::template_manager::template_manager(ctx::context_manager_impl* ctx_mgr, ctx::rule_manager* rule_mgr)
: _context_mgr(ctx_mgr), _rule_mgr(rule_mgr)
{
}

ctx::template_manager::~template_manager()
{
	apply_templates();
}

bool ctx::template_manager::init()
{
	std::string q = std::string("CREATE TABLE IF NOT EXISTS context_trigger_template ")
			+ "(name TEXT DEFAULT '' NOT NULL PRIMARY KEY, operation INTEGER DEFAULT 3 NOT NULL, "
			+ "attributes TEXT DEFAULT '' NOT NULL, options TEXT DEFAULT '' NOT NULL, owner TEXT DEFAULT '' NOT NULL)";

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, false, _E, "Create template table failed");

	// Apply templates
	apply_templates();

	return true;
}

void ctx::template_manager::apply_templates()
{
	// TODO remove templates if needed
	std::string subject;
	int operation;
	ctx::json attributes;
	ctx::json options;
	std::string owner;
	bool unregister;
	std::string query;
	query.clear();

	while(_context_mgr->pop_trigger_item(subject, operation, attributes, options, owner, unregister)) {
		if (unregister) {
			query += remove_template(subject);
			_rule_mgr->pause_rule_with_item(subject);
		} else {
			query += add_template(subject, operation, attributes, options, owner);
			if (!owner.empty()) {
				_rule_mgr->resume_rule_with_item(subject);
			}
		}
	}
	std::vector<json> record;
	bool ret = db_manager::execute_sync(query.c_str(), &record);
	IF_FAIL_VOID_TAG(ret, _E, "Update template db failed");
}

std::string ctx::template_manager::add_template(std::string &subject, int &operation, ctx::json &attributes, ctx::json &options, std::string &owner)
{
	_D("[Add template] Subject: %s, Ops: %d, Owner: %s", subject.c_str(), operation, owner.c_str());
	_J("Attr", attributes);
	_J("Opt", options);

	std::string query = "UPDATE context_trigger_template SET operation=" + int_to_string(operation)
			+ ", attributes='" + attributes.str() + "', options='" + options.str() + "', owner='" + owner
			+ "' WHERE name='" + subject + "'; ";

	query += "INSERT OR IGNORE INTO context_trigger_template (name, operation, attributes, options, owner) VALUES ('"
			+ subject + "', " + int_to_string(operation) + ", '" + attributes.str() + "', '" + options.str() + "', '"
			+ owner + "'); ";

	return query;
}

std::string ctx::template_manager::remove_template(std::string &subject)
{
	_D("[Remove template] Subject: %s", subject.c_str());
	std::string query = "DELETE FROM context_trigger_template WHERE name = '" + subject + "'; ";

	return query;
}

int ctx::template_manager::get_template(std::string &subject, ctx::json* tmpl)
{
	// Update latest template information
	apply_templates();

	std::string q = "SELECT * FROM context_trigger_template WHERE name = '" + subject + "'";

	std::vector<json> record;
	bool ret = db_manager::execute_sync(q.c_str(), &record);
	IF_FAIL_RETURN_TAG(ret, ERR_OPERATION_FAILED, _E, "Query template failed");
	IF_FAIL_RETURN_TAG(record.size() > 0, ERR_NOT_SUPPORTED, _E, "Template(%s) not found", subject.c_str());
	IF_FAIL_RETURN_TAG(record.size() == 1, ERR_OPERATION_FAILED, _E, "Tepmlate duplicated");

	(*tmpl) = *record.begin();

	std::string opt_str;
	std::string attr_str;
	tmpl->get(NULL, TYPE_OPTION_STR, &opt_str);
	tmpl->get(NULL, TYPE_ATTR_STR, &attr_str);

	ctx::json opt = opt_str;
	ctx::json attr = attr_str;

	tmpl->set(NULL, TYPE_OPTION_STR, opt);
	tmpl->set(NULL, TYPE_ATTR_STR, attr);

	return ERR_NONE;
}
