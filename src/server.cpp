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

#include <stdlib.h>
#include <new>
#include <glib.h>
#include <glib-object.h>

#include <types_internal.h>
#include "DBusServer.h"
#include "db_mgr_impl.h"
#include "timer_mgr_impl.h"
#include "context_mgr_impl.h"
#include "context_trigger/trigger.h"
#include "server.h"

static GMainLoop *mainloop = NULL;
static bool started = false;

static ctx::context_manager_impl *context_mgr = NULL;
static ctx::timer_manager_impl *timer_mgr = NULL;
static ctx::db_manager_impl *database_mgr = NULL;
static ctx::DBusServer *dbus_handle = NULL;
static ctx::context_trigger *trigger = NULL;

/* TODO: re-organize activation & deactivation processes */
void ctx::server::initialize()
{
	_I("Init MainLoop");
	mainloop = g_main_loop_new(NULL, FALSE);

	_I("Init Dbus Connection");
	dbus_handle = new(std::nothrow) ctx::DBusServer();
	IF_FAIL_VOID_TAG(dbus_handle, _E, "Memory allocation failed");
	IF_FAIL_VOID_TAG(dbus_handle->__init(), _E, "Initialization Failed");

	// Start the main loop
	_I(CYAN("Launching Context-Service"));
	g_main_loop_run(mainloop);
}

void ctx::server::activate()
{
	IF_FAIL_VOID(!started);

	bool result = false;

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

	started = true;
	_I(CYAN("Context-Service Launched"));
	return;

CATCH:
	_E(RED("Launching Failed"));

	// Stop the main loop
	g_main_loop_quit(mainloop);
}

void ctx::server::release()
{
	_I(CYAN("Terminating Context-Service"));
	_I("Release Context Trigger");
	if (trigger)
		trigger->release();

	_I("Release Analyzer Manager");
	if (context_mgr)
		context_mgr->release();

	_I("Release Dbus Connection");
	if (dbus_handle)
		dbus_handle->__release();

	_I("Close the Database");
	if (database_mgr)
		database_mgr->release();

	_I("Release Timer Manager");
	if (timer_mgr)
		timer_mgr->release();

	g_main_loop_unref(mainloop);

	delete trigger;
	delete context_mgr;
	delete dbus_handle;
	delete database_mgr;
	delete timer_mgr;
}

static gboolean postpone_request_assignment(gpointer data)
{
	ctx::server::send_request(static_cast<ctx::request_info*>(data));
	return FALSE;
}

void ctx::server::send_request(ctx::request_info* request)
{
	if (!started) {
		_W("Service not ready...");
		g_idle_add(postpone_request_assignment, request);
		return;
	}

	if (!trigger->assign_request(request)) {
		context_mgr->assign_request(request);
	}
}

static void signal_handler(int signo)
{
	_I("SIGNAL %d received", signo);

	// Stop the main loop
	g_main_loop_quit(mainloop);
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
	sigaction(SIGABRT, &signal_action, NULL);

#if !defined(GLIB_VERSION_2_36)
	g_type_init();
#endif

	ctx::server::initialize();
	ctx::server::release();

	return EXIT_SUCCESS;
}
