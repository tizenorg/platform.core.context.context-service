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

#include <Types.h>
#include "DBusServer.h"
#include "ContextManager.h"
#include "policy/PolicyManager.h"
#include "Server.h"

#ifdef TRIGGER_SUPPORT
#include "trigger/Trigger.h"
#endif

static GMainLoop *mainloop = NULL;
static bool started = false;

static ctx::ContextManager *__contextMgr = NULL;
static ctx::DBusServer *__dbusHandle = NULL;
static ctx::PolicyManager *__policyMgr = NULL;

#ifdef TRIGGER_SUPPORT
static ctx::trigger::Trigger *__contextTrigger = NULL;
#endif

/* TODO: re-organize activation & deactivation processes */
void ctx::Server::initialize()
{
	_I("Init MainLoop");
	mainloop = g_main_loop_new(NULL, FALSE);

	_I("Init Dbus Connection");
	__dbusHandle = new(std::nothrow) ctx::DBusServer();
	IF_FAIL_VOID_TAG(__dbusHandle, _E, "Memory allocation failed");
	IF_FAIL_VOID_TAG(__dbusHandle->__init(), _E, "Initialization Failed");

	// Start the main loop
	_I(CYAN("Launching Context-Service"));
	g_main_loop_run(mainloop);
}

void ctx::Server::activate()
{
	IF_FAIL_VOID(!started);

	bool result = false;

	_I("Init Context Manager");
	__contextMgr = new(std::nothrow) ctx::ContextManager();
	IF_FAIL_CATCH_TAG(__contextMgr, _E, "Memory allocation failed");
	result = __contextMgr->init();
	IF_FAIL_CATCH_TAG(result, _E, "Initialization Failed");

#ifdef TRIGGER_SUPPORT
	_I("Init Context Trigger");
	__contextTrigger = new(std::nothrow) ctx::trigger::Trigger();
	IF_FAIL_CATCH_TAG(__contextTrigger, _E, "Memory allocation failed");
	result = __contextTrigger->init(__contextMgr);
	IF_FAIL_CATCH_TAG(result, _E, "Initialization Failed");
#endif

	_I("Init Policy Manager");
	__policyMgr = new(std::nothrow) ctx::PolicyManager(__contextMgr);
	IF_FAIL_CATCH_TAG(__policyMgr, _E, "Memory allocation failed");

	started = true;
	_I(CYAN("Context-Service Launched"));
	return;

CATCH:
	_E(RED("Launching Failed"));

	// Stop the main loop
	g_main_loop_quit(mainloop);
}

void ctx::Server::release()
{
	_I(CYAN("Terminating Context-Service"));

	_I("Release Policy Manager");
	delete __policyMgr;

#ifdef TRIGGER_SUPPORT
	_I("Release Context Trigger");
	if (__contextTrigger)
		__contextTrigger->release();
#endif

	_I("Release Context Manager");
	if (__contextMgr)
		__contextMgr->release();

	_I("Release Dbus Connection");
	if (__dbusHandle)
		__dbusHandle->__release();

	g_main_loop_unref(mainloop);

#ifdef TRIGGER_SUPPORT
	delete __contextTrigger;
#endif
	delete __contextMgr;
	delete __dbusHandle;
}

static gboolean __postponeRequestAssignment(gpointer data)
{
	ctx::Server::sendRequest(static_cast<ctx::RequestInfo*>(data));
	return FALSE;
}

void ctx::Server::sendRequest(ctx::RequestInfo* request)
{
	if (!started) {
		_W("Service not ready...");
		g_idle_add(__postponeRequestAssignment, request);
		return;
	}

#ifdef TRIGGER_SUPPORT
	if (__contextTrigger->assignRequest(request))
		return;
#endif

	__contextMgr->assignRequest(request);
}

static void __signalHandler(int signo)
{
	_I("SIGNAL %d received", signo);

	// Stop the main loop
	g_main_loop_quit(mainloop);
}

int main(int argc, char* argv[])
{
	static struct sigaction signalAction;
	signalAction.sa_handler = __signalHandler;
	sigemptyset(&signalAction.sa_mask);

	sigaction(SIGINT, &signalAction, NULL);
	sigaction(SIGHUP, &signalAction, NULL);
	sigaction(SIGTERM, &signalAction, NULL);
	sigaction(SIGQUIT, &signalAction, NULL);
	sigaction(SIGABRT, &signalAction, NULL);

#if !defined(GLIB_VERSION_2_36)
	g_type_init();
#endif

	ctx::Server::initialize();
	ctx::Server::release();

	return EXIT_SUCCESS;
}
