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

#include <Types.h>
#include <Json.h>
#include <app_control.h>
#include <app_control_internal.h>
#include <bundle.h>
#include <device/display.h>
#include <notification.h>
#include <notification_internal.h>
#include <vconf.h>
#include <context_trigger_types_internal.h>
#include "../DBusServer.h"
#include "ActionManager.h"

static void __triggerActionAppControl(ctx::Json& action);
static void __triggerActionNotification(ctx::Json& action, std::string pkgId);
static void __triggerActionDbusCall(ctx::Json& action);
//LCOV_EXCL_START
void ctx::trigger::action_manager::triggerAction(ctx::Json& action, std::string pkgId)
{
	std::string type;
	action.get(NULL, CT_RULE_ACTION_TYPE, &type);

	if (type.compare(CT_RULE_ACTION_TYPE_APP_CONTROL) == 0) {
		__triggerActionAppControl(action);
	} else if (type.compare(CT_RULE_ACTION_TYPE_NOTIFICATION) == 0) {
		__triggerActionNotification(action, pkgId);
	} else if (type.compare(CT_RULE_ACTION_TYPE_DBUS_CALL) == 0) {
		__triggerActionDbusCall(action);
	}
}

void __triggerActionAppControl(ctx::Json& action)
{
	int error;
	std::string appctlStr;
	action.get(NULL, CT_RULE_ACTION_APP_CONTROL, &appctlStr);

	char* str = static_cast<char*>(malloc(appctlStr.length()));
	if (str == NULL) {
		_E("Memory allocation failed");
		return;
	}
	appctlStr.copy(str, appctlStr.length(), 0);
	bundle_raw* encoded = reinterpret_cast<unsigned char*>(str);
	bundle* appctlBundle = bundle_decode(encoded, appctlStr.length());

	app_control_h app = NULL;
	app_control_create(&app);
	app_control_import_from_bundle(app, appctlBundle);

	error = app_control_send_launch_request(app, NULL, NULL);
	if (error != APP_CONTROL_ERROR_NONE) {
		_E("Launch request failed(%d)", error);
	} else {
		_D("Launch request succeeded");
	}
	bundle_free(appctlBundle);
	free(str);
	app_control_destroy(app);

	error = device_display_change_state(DISPLAY_STATE_NORMAL);
	if (error != DEVICE_ERROR_NONE) {
		_E("Change display state failed(%d)", error);
	}
}

void __triggerActionNotification(ctx::Json& action, std::string pkgId)
{
	int error;
	notification_h notification = notification_create(NOTIFICATION_TYPE_NOTI);
	std::string title;
	if (action.get(NULL, CT_RULE_ACTION_NOTI_TITLE, &title)) {
		error = notification_set_text(notification, NOTIFICATION_TEXT_TYPE_TITLE, title.c_str(), NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification title failed(%d)", error);
		}
	}

	std::string content;
	if (action.get(NULL, CT_RULE_ACTION_NOTI_CONTENT, &content)) {
		error = notification_set_text(notification, NOTIFICATION_TEXT_TYPE_CONTENT, content.c_str(), NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification contents failed(%d)", error);
		}
	}

	std::string imagePath;
	if (action.get(NULL, CT_RULE_ACTION_NOTI_ICON_PATH, &imagePath)) {
		error = notification_set_image(notification, NOTIFICATION_IMAGE_TYPE_ICON, imagePath.c_str());
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification icon image failed(%d)", error);
		}
	}

	std::string appctlStr;
	char* str = NULL;
	bundle_raw* encoded = NULL;
	bundle* appctlBundle = NULL;
	app_control_h app = NULL;
	if (action.get(NULL, CT_RULE_ACTION_APP_CONTROL, &appctlStr)) {
		str = static_cast<char*>(malloc(appctlStr.length()));
		if (str == NULL) {
			_E("Memory allocation failed");
			notification_free(notification);
			return;
		}
		appctlStr.copy(str, appctlStr.length(), 0);
		encoded = reinterpret_cast<unsigned char*>(str);
		appctlBundle = bundle_decode(encoded, appctlStr.length());

		app_control_create(&app);
		app_control_import_from_bundle(app, appctlBundle);

		error = notification_set_launch_option(notification, NOTIFICATION_LAUNCH_OPTION_APP_CONTROL, app);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set launch option failed(%d)", error);
		}
	}

	if (!pkgId.empty()) {
		error = notification_set_pkgname(notification, pkgId.c_str());
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set package id(%s) failed(%#x)", pkgId.c_str(), error);
		}
	}

	int soundOn = 0;
	error = vconf_get_bool(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL, &soundOn);
	if (error < 0) {
		_E("vconf error (%d)", error);
	} else if (soundOn) {
		error = notification_set_sound(notification, NOTIFICATION_SOUND_TYPE_DEFAULT, NULL);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification sound failed(%d)", error);
		}
	}

	int vibrationOn = 0;
	error = vconf_get_bool(VCONFKEY_SETAPPL_VIBRATION_STATUS_BOOL, &vibrationOn);
	if (error < 0) {
		_E("vconf error (%d)", error);
	} else if (vibrationOn) {
		error = notification_set_vibration(notification, NOTIFICATION_VIBRATION_TYPE_DEFAULT, NULL);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification vibration failed(%d)", error);
		}
	}

	error = notification_post(notification);
	if (error != NOTIFICATION_ERROR_NONE) {
		_E("Post notification failed(%d)", error);
	} else {
		_D("Post notification succeeded");
	}

	bundle_free(appctlBundle);
	free(str);
	notification_free(notification);
	if (app) {
		app_control_destroy(app);
	}

	error = device_display_change_state(DISPLAY_STATE_NORMAL);
	if (error != DEVICE_ERROR_NONE) {
		_E("Change display state failed(%d)", error);
	}
}

void __triggerActionDbusCall(ctx::Json& action)
{
	std::string busName, object, iface, method;
	GVariant *param = NULL;

	action.get(NULL, CT_RULE_ACTION_DBUS_NAME, &busName);
	IF_FAIL_VOID_TAG(!busName.empty(), _E, "No target bus name");

	action.get(NULL, CT_RULE_ACTION_DBUS_OBJECT, &object);
	IF_FAIL_VOID_TAG(!object.empty(), _E, "No object path");

	action.get(NULL, CT_RULE_ACTION_DBUS_INTERFACE, &iface);
	IF_FAIL_VOID_TAG(!iface.empty(), _E, "No interface name");

	action.get(NULL, CT_RULE_ACTION_DBUS_METHOD, &method);
	IF_FAIL_VOID_TAG(!method.empty(), _E, "No method name");

	action.get(NULL, CT_RULE_ACTION_DBUS_PARAMETER, &param);

	ctx::DBusServer::call(busName, object, iface, method, param);
}
//LCOV_EXCL_STOP
