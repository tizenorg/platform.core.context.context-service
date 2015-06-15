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

#include <types_internal.h>
#include <json.h>
#include <string>
#include <sstream>
#include <cstdlib>
#include "clips_handler.h"
#include "rule_manager.h"

extern "C"
{
#include <clips/clips.h>
}

static void* env = NULL;
static ctx::rule_manager* rule_mgr = NULL;
static int string_to_int(std::string str);

ctx::clips_handler::clips_handler()
{
}

ctx::clips_handler::~clips_handler()
{
	if (env) {
		DestroyEnvironment(env);
	}
}

bool ctx::clips_handler::init(ctx::rule_manager* rm)
{
	rule_mgr = rm;

	int error = init_environment(env);
	IF_FAIL_RETURN(error == ERR_NONE, false);

	bool ret = define_global_variable_string("zone", "");
	IF_FAIL_RETURN(ret, false);

	return true;
}

int ctx::clips_handler::init_environment(void* &environment)
{
	environment = CreateEnvironment();
	if (!environment) {
		_E("Create environment failed");
		return ERR_OPERATION_FAILED;
	}

	char* func_name = strdup("execute_action");
	char* restrictions = strdup("1s");

	if (func_name == NULL || restrictions == NULL) {
		_E("Memory allocation failed");
		free(func_name);
		free(restrictions);
		return ERR_OUT_OF_MEMORY;
	}

	EnvDefineFunction2(environment, func_name, 'i', PTIEF execute_action, func_name, restrictions);

	free(func_name);
	free(restrictions);

	return ERR_NONE;
}

int ctx::clips_handler::define_template(std::string& script)
{
	IF_FAIL_RETURN_TAG(env_build(script) == ERR_NONE, ERR_OPERATION_FAILED, _E, "Deftemplate failed");
	return ERR_NONE;
}

int ctx::clips_handler::define_class(std::string& script)
{
	IF_FAIL_RETURN_TAG(env_build(script) == ERR_NONE, ERR_OPERATION_FAILED, _E, "Defclass failed");
	return ERR_NONE;
}

int ctx::clips_handler::define_rule(std::string& script)
{
	IF_FAIL_RETURN_TAG(env_build(script) == ERR_NONE, ERR_OPERATION_FAILED, _E, "Defrule failed");
	return ERR_NONE;
}

int ctx::clips_handler::env_build(std::string& script)
{
	ASSERT_NOT_NULL(env);
	if (script.length() == 0)
		return ERR_INVALID_PARAMETER;

	_I("EnvBuild script (%s)", script.c_str());
	int ret = EnvBuild(env, script.c_str());

	return (ret == 1)? ERR_NONE : ERR_OPERATION_FAILED;
}

int ctx::clips_handler::run_environment()
{
	ASSERT_NOT_NULL(env);

	int fired_rule_num = EnvRun(env, -1);
	IF_FAIL_RETURN(fired_rule_num >= 0, ERR_OPERATION_FAILED);

	return ERR_NONE;
}

int ctx::clips_handler::add_fact(std::string& fact)
{
	ASSERT_NOT_NULL(env);
	if (fact.length() == 0)
		return ERR_INVALID_PARAMETER;

	void* assert_fact = EnvAssertString(env, fact.c_str());
	IF_FAIL_RETURN_TAG(assert_fact, ERR_OPERATION_FAILED, _E, "Fact assertion failed");

	return ERR_NONE;
}

int ctx::clips_handler::route_string_command(std::string& script)
{
	ASSERT_NOT_NULL(env);
	if (script.length() == 0)
		return ERR_INVALID_PARAMETER;

	int error;
	if (RouteCommand(env, script.c_str(), TRUE)){
		_D("Route command succeeded(%s).", script.c_str());
		error = ERR_NONE;
	} else {
		_E("Route command failed");
		error = ERR_OPERATION_FAILED;
	}

	return error;
}

int ctx::clips_handler::make_instance(std::string& script)
{
	ASSERT_NOT_NULL(env);
	if (script.length() == 0)
		return ERR_INVALID_PARAMETER;

	int error;
	if (EnvMakeInstance(env, script.c_str())){
		_D("Make instance succeeded - %s", script.c_str());
		error = ERR_NONE;
	} else {
		_E("Make instance failed");
		error = ERR_OPERATION_FAILED;
	}

	return error;
}

int ctx::clips_handler::unmake_instance(std::string& instance_name)
{
	ASSERT_NOT_NULL(env);
	if (instance_name.length() == 0)
		return ERR_INVALID_PARAMETER;

	void* instance = find_instance_internal(instance_name);
	if (!instance) {
		_E("Cannot find instance(%s).", instance_name.c_str());
		return ERR_INVALID_PARAMETER;
	}

	if (!EnvUnmakeInstance(env, instance)){
		_E("Unmake instance failed");
		return ERR_OPERATION_FAILED;
	}

	_D("Unmake instance succeeded(%s).", instance_name.c_str());
	return ERR_NONE;
}

int ctx::clips_handler::execute_action()
{
	if (!env) {
		_E("Environment not created");
		return ERR_OPERATION_FAILED;
	}

	const char* result = EnvRtnLexeme(env, 1);
	if (!result) {
		_E("Failed to get returned rule id");
		return ERR_OPERATION_FAILED;
	}
	std::string rule_id = result;
	std::string id_str = rule_id.substr(4);

	int id = string_to_int(id_str);
	rule_mgr->on_rule_triggered(id);

	return ERR_NONE;
}

bool ctx::clips_handler::find_instance(std::string& instance_name)
{
	ASSERT_NOT_NULL(env);
	if (find_instance_internal(instance_name)) {
		_D("[%s] already exists", instance_name.c_str());
		return true;
	}

	return false;
}

void* ctx::clips_handler::find_instance_internal(std::string& instance_name)
{
	void* instance = EnvFindInstance(env, NULL, instance_name.c_str(), TRUE);
	return instance;
}

int string_to_int(std::string str)
{
	int i;
	std::istringstream convert(str);

	if (!(convert >> i))
		i = 0;

	return i;
}

bool ctx::clips_handler::define_global_variable_string(std::string variable_name, std::string value)
{
	std::string script = "(defglobal ?*";
	script += variable_name;
	script += "* = \"";
	script += value;
	script += "\")";

	IF_FAIL_RETURN_TAG(env_build(script) == ERR_NONE, false, _E, "Defglobal failed");
	return true;
}

bool ctx::clips_handler::set_global_variable_string(std::string variable_name, std::string value)
{
	ASSERT_NOT_NULL(env);
	if (variable_name.length() == 0)
		return false;

	DATA_OBJECT data;
	SetType(data, STRING);
	SetValue(data, EnvAddSymbol(env, value.c_str())) ;

	int ret = EnvSetDefglobalValue(env, variable_name.c_str(), &data);

	IF_FAIL_RETURN_TAG(ret == 1, false, _E, "Set global variable(%s) failed", variable_name.c_str());
	return true;
}
