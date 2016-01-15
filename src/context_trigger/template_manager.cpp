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
#include "template_manager.h"

static std::string int_to_string(int i)
{
	std::ostringstream convert;
	convert << i;
	std::string str = convert.str();
	return str;
}

ctx::template_manager::template_manager(ctx::context_manager_impl* ctx_mgr)
: _context_mgr(ctx_mgr)
{
}

ctx::template_manager::~template_manager()
{
}

bool ctx::template_manager::get_fact_definition(std::string &subject, int &operation, ctx::json &attributes, ctx::json &options)
{
	return _context_mgr->pop_trigger_item(subject, operation, attributes, options);
}

void ctx::template_manager::apply_templates(void)
{
	// TODO remove templates if needed
	std::string subject;
	int operation;
	ctx::json attributes;
	ctx::json options;
	std::string q_update;
	std::string q_insert = "INSERT OR IGNORE INTO context_trigger_template (name, operation, attributes, options) VALUES";
	int cnt = 0;

	while (get_fact_definition(subject, operation, attributes, options)) {
		_D("Subject: %s, Ops: %d", subject.c_str(), operation);
		_J("Attr", attributes);
		_J("Opt", options);

		q_update += "UPDATE context_trigger_template SET operation=" + int_to_string(operation)
			+ ", attributes='" + attributes.str() + "', options='" + options.str() + "' WHERE name='" + subject + "';";

		q_insert += " ('" + subject + "', " + int_to_string(operation) + ", '" + attributes.str() + "', '" + options.str() + "'),";
		cnt++;
	}
	IF_FAIL_VOID(cnt > 0);

	q_insert.erase(q_insert.end() - 1, q_insert.end());
	q_insert += ";";

	bool ret = db_manager::execute(1, q_update.c_str(), NULL);
	IF_FAIL_VOID_TAG(ret, _E, "Update item definition failed");

	ret = db_manager::execute(2, q_insert.c_str(), NULL);
	IF_FAIL_VOID_TAG(ret, _E, "Insert item definition failed");
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
