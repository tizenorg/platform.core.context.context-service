/*
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
 */

//#define _USE_ECORE_MAIN_LOOP_

#include <stdlib.h>
#include <new>
#include <glib.h>
#include <glib-object.h>

#include <types_internal.h>
#include "dbus_server_impl.h"
#include "db_mgr_impl.h"
#include "timer_mgr_impl.h"
#include "context_mgr_impl.h"
#include "zone_util_impl.h"
#include "access_control/privilege.h"
#include "context_trigger/trigger.h"
#include "server.h"

#ifdef _USE_ECORE_MAIN_LOOP_
#include <Ecore.h>
#else
static GMainLoop *mainloop = NULL;
#endif

static bool started = false;

static ctx::context_manager_impl *context_mgr = NULL;
static ctx::timer_manager_impl *timer_mgr = NULL;
static ctx::db_manager_impl *database_mgr = NULL;
static ctx::dbus_server_impl *dbus_handle = NULL;
static ctx::context_trigger *trigger = NULL;

void ctx::server::run()
{
	if (started) {
		_W("Started already");
		return;
	}

	_I("Init MainLoop");
#ifdef _USE_ECORE_MAIN_LOOP_
	ecore_init();
	ecore_main_loop_glib_integrate();
#else
	mainloop = g_main_loop_new(NULL, FALSE);
#endif

	bool result = false;

	_I("Init vasum context");
	result = ctx::zone_util::init();
	IF_FAIL_CATCH_TAG(result, _E, "Vasum context initialization failed");

	_I("Init access control configuration");
	result = ctx::privilege_manager::init();
	IF_FAIL_CATCH_TAG(result, _E, "Access controller initialization failed");

	_I("Init Timer Manager");
	timer_mgr = new(std::nothrow) ctx::timer_manager_impl();
	IF_FAIL_CATCH_TAG(timer_mgr, _E, "Memory allocation failed");
	timer_manager::set_instance(timer_mgr);
	result = timer_mgr->init();
	IF_FAIL_CATCH_TAG(result, _E, "Initialization Failed");

	_I("Init Database Manager");
	database_mgr = new(std::nothrow) ctx::db_manager_impl();
	IF_FAIL_CATCH_TAG(database_mgr, _E, "Memory allocation failed");
	db_manager::set_instance(database_mgr);
	result = database_mgr->init();
	IF_FAIL_CATCH_TAG(result, _E, "Initialization Failed");

	_I("Init Context Manager");
	context_mgr = new(std::nothrow) ctx::context_manager_impl();
	IF_FAIL_CATCH_TAG(context_mgr, _E, "Memory allocation failed");
	context_manager::set_instance(context_mgr);
	result = context_mgr->init();
	IF_FAIL_CATCH_TAG(result, _E, "Initialization Failed");

	_I("Init Context Trigger");
	trigger = new(std::nothrow) ctx::context_trigger();
	IF_FAIL_CATCH_TAG(trigger, _E, "Memory allocation failed");
	result = trigger->init(context_mgr);
	IF_FAIL_CATCH_TAG(result, _E, "Initialization Failed");

	_I("Init Dbus Connection");
	dbus_handle = new(std::nothrow) ctx::dbus_server_impl();
	IF_FAIL_CATCH_TAG(dbus_handle, _E, "Memory allocation failed");
	dbus_server::set_instance(dbus_handle);
	result = dbus_handle->init();
	IF_FAIL_CATCH_TAG(result, _E, "Initialization Failed");

	// Start the main loop
	started = true;
	_I(CYAN("Context-Service Launched"));
#ifdef _USE_ECORE_MAIN_LOOP_
	ecore_main_loop_begin();
#else
	g_main_loop_run(mainloop);
#endif

	_I(CYAN("Terminating Context-Service"));

	_I("Release Context Trigger");
	trigger->release();

	_I("Release Analyzer Manager");
	context_mgr->release();

	_I("Release Dbus Connection");
	dbus_handle->release();

	_I("Close the Database");
	database_mgr->release();

	_I("Release Timer Manager");
	timer_mgr->release();

	_I("Release Access control configuration");
	ctx::privilege_manager::release();

	_I("Release Vasum context");
	ctx::zone_util::release();

#ifdef _USE_ECORE_MAIN_LOOP_
	ecore_shutdown();
#else
	g_main_loop_unref(mainloop);
#endif

	delete trigger;
	delete context_mgr;
	delete dbus_handle;
	delete database_mgr;
	delete timer_mgr;
	return;

CATCH:
	_E(RED("Launching Failed"));
	delete trigger;
	delete context_mgr;
	delete dbus_handle;
	delete database_mgr;
	delete timer_mgr;
}

void ctx::server::send_request(ctx::request_info* request)
{
	if (!trigger->assign_request(request)) {
		context_mgr->assign_request(request);
	}
}

static void signal_handler(int signo)
{
	_I("SIGNAL %d received", signo);

	// Stop the main loop
#ifdef _USE_ECORE_MAIN_LOOP_
	ecore_main_loop_quit();
#else
	g_main_loop_quit(mainloop);
#endif
}

int main(int argc, char* argv[])
{
	static struct sigaction signal_action;
	signal_action.sa_handler = signal_handler;
	sigemptyset(&signal_action.sa_mask);

	sigaction(SIGINT, &signal_action, NULL);
	sigaction(SIGHUP, &signal_action, NULL);
	sigaction(SIGTERM, &signal_action, NULL);
	sigaction(SIGQUIT, &signal_action, NULL);

#if !defined(GLIB_VERSION_2_36)
	g_type_init();
#endif

	ctx::server::run();

	return EXIT_SUCCESS;
}
