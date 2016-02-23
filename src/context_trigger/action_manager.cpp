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

#include <app_control.h>
#include <app_control_internal.h>
#include <bundle.h>
#include <device/display.h>
#include <notification.h>
#include <notification_internal.h>
#include <runtime_info.h>
#include <system_settings.h>
#include <context_trigger_types_internal.h>
#include <Json.h>
#include "../DBusServer.h"
#include "action_manager.h"

static void trigger_action_app_control(ctx::Json& action);
static void trigger_action_notification(ctx::Json& action, std::string pkg_id);
static void trigger_action_dbus_call(ctx::Json& action);

void ctx::action_manager::trigger_action(ctx::Json& action, std::string pkg_id)
{
	std::string type;
	action.get(NULL, CT_RULE_ACTION_TYPE, &type);

	if (type.compare(CT_RULE_ACTION_TYPE_APP_CONTROL) == 0) {
		trigger_action_app_control(action);
	} else if (type.compare(CT_RULE_ACTION_TYPE_NOTIFICATION) == 0) {
		trigger_action_notification(action, pkg_id);
	} else if (type.compare(CT_RULE_ACTION_TYPE_DBUS_CALL) == 0) {
		trigger_action_dbus_call(action);
	}
}

void trigger_action_app_control(ctx::Json& action)
{
	int error;
	std::string appctl_str;
	action.get(NULL, CT_RULE_ACTION_APP_CONTROL, &appctl_str);

	char* str = static_cast<char*>(malloc(appctl_str.length()));
	if (str == NULL) {
		_E("Memory allocation failed");
		return;
	}
	appctl_str.copy(str, appctl_str.length(), 0);
	bundle_raw* encoded = reinterpret_cast<unsigned char*>(str);
	bundle* appctl_bundle = bundle_decode(encoded, appctl_str.length());

	app_control_h app = NULL;
	app_control_create(&app);
	app_control_import_from_bundle(app, appctl_bundle);

	error = app_control_send_launch_request(app, NULL, NULL);
	if (error != APP_CONTROL_ERROR_NONE) {
		_E("Launch request failed(%d)", error);
	} else {
		_D("Launch request succeeded");
	}
	bundle_free(appctl_bundle);
	free(str);
	app_control_destroy(app);

	error = device_display_change_state(DISPLAY_STATE_NORMAL);
	if (error != DEVICE_ERROR_NONE) {
		_E("Change display state failed(%d)", error);
	}
}

void trigger_action_notification(ctx::Json& action, std::string pkg_id)
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

	std::string image_path;
	if (action.get(NULL, CT_RULE_ACTION_NOTI_ICON_PATH, &image_path)) {
		error = notification_set_image(notification, NOTIFICATION_IMAGE_TYPE_ICON, image_path.c_str());
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification icon image failed(%d)", error);
		}
	}

	std::string appctl_str;
	char* str = NULL;
	bundle_raw* encoded = NULL;
	bundle* appctl_bundle = NULL;
	app_control_h app = NULL;
	if (action.get(NULL, CT_RULE_ACTION_APP_CONTROL, &appctl_str)) {
		str = static_cast<char*>(malloc(appctl_str.length()));
		if (str == NULL) {
			_E("Memory allocation failed");
			notification_free(notification);
			return;
		}
		appctl_str.copy(str, appctl_str.length(), 0);
		encoded = reinterpret_cast<unsigned char*>(str);
		appctl_bundle = bundle_decode(encoded, appctl_str.length());

		app_control_create(&app);
		app_control_import_from_bundle(app, appctl_bundle);

		error = notification_set_launch_option(notification, NOTIFICATION_LAUNCH_OPTION_APP_CONTROL, app);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set launch option failed(%d)", error);
		}
	}

	if (!pkg_id.empty()) {
		error = notification_set_pkgname(notification, pkg_id.c_str());
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set package id(%s) failed(%#x)", pkg_id.c_str(), error);
		}
	}

	bool silent = true;
	error = system_settings_get_value_bool(SYSTEM_SETTINGS_KEY_SOUND_SILENT_MODE, &silent);
	if (error != SYSTEM_SETTINGS_ERROR_NONE) {
		_E("Get system setting(silent mode) failed(%d)", error);
	}

	bool vibration = true;
	error = runtime_info_get_value_bool(RUNTIME_INFO_KEY_VIBRATION_ENABLED, &vibration);
	if (error != RUNTIME_INFO_ERROR_NONE) {
		_E("Get runtime info(vibration) failed(%d)", error);
	}

	if (!silent) {
	    error = notification_set_sound(notification, NOTIFICATION_SOUND_TYPE_DEFAULT, NULL);
		if (error != NOTIFICATION_ERROR_NONE) {
			_E("Set notification sound failed(%d)", error);
		}

		if (vibration) {
			error = notification_set_vibration(notification, NOTIFICATION_VIBRATION_TYPE_DEFAULT, NULL);
			if (error != NOTIFICATION_ERROR_NONE) {
				_E("Set notification vibration failed(%d)", error);
			}
		}
	}

	error = notification_post(notification);
	if (error != NOTIFICATION_ERROR_NONE) {
		_E("Post notification failed(%d)", error);
	} else {
		_D("Post notification succeeded");
	}

	bundle_free(appctl_bundle);
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

void trigger_action_dbus_call(ctx::Json& action)
{
	std::string bus_name, object, iface, method;
	GVariant *param = NULL;

	action.get(NULL, CT_RULE_ACTION_DBUS_NAME, &bus_name);
	IF_FAIL_VOID_TAG(!bus_name.empty(), _E, "No target bus name");

	action.get(NULL, CT_RULE_ACTION_DBUS_OBJECT, &object);
	IF_FAIL_VOID_TAG(!object.empty(), _E, "No object path");

	action.get(NULL, CT_RULE_ACTION_DBUS_INTERFACE, &iface);
	IF_FAIL_VOID_TAG(!iface.empty(), _E, "No interface name");

	action.get(NULL, CT_RULE_ACTION_DBUS_METHOD, &method);
	IF_FAIL_VOID_TAG(!method.empty(), _E, "No method name");

	action.get(NULL, CT_RULE_ACTION_DBUS_PARAMETER, &param);

	ctx::DBusServer::call(bus_name, object, iface, method, param);
}
