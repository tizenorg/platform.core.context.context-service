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

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <types_internal.h>

#include "privilege.h"
#include "config_loader.h"

#define CONFIG_PATH "/opt/data/context-service/access-config.xml"
#define ELM_ROOT "ContextAccessConfig"
#define ELM_PRIV "Privilege"
#define ELM_LINK "Link"
#define ELM_ALLOW "Allow"
#define ATTR_NAME "name"
#define ATTR_SUBJ "subject"
#define IS_NODE_OF(node, element) (xmlStrcmp((node)->name, (const xmlChar *)(element)) == 0)

static void parse_priv_node(xmlNodePtr cursor)
{
	char *prop = (char*)(xmlGetProp(cursor, (const xmlChar*)ATTR_NAME));
	IF_FAIL_VOID_TAG(prop, _E, "Failed to get '%s'", ATTR_NAME);

	std::string name = prop;
	free(prop);

	cursor = cursor->xmlChildrenNode;
	IF_FAIL_VOID_TAG(cursor, _E, "No child node exists");

	while (cursor) {
		if (!IS_NODE_OF(cursor, ELM_ALLOW)) {
			_D("Skipping a node '%s'", (const char*)cursor->name);
			cursor = cursor->next;
			continue;
		}

		prop = (char*)(xmlGetProp(cursor, (const xmlChar*)ATTR_SUBJ));
		if (prop == NULL) {
			_E("Failed to get '%s'", ATTR_SUBJ);
			cursor = cursor->next;
			continue;
		}

		_SI("Set Privilege: %s <- %s", prop, name.c_str());
		ctx::privilege_manager::set(prop, name.c_str());

		free(prop);
		cursor = cursor->next;
	}
}

static void parse_link_node(xmlNodePtr cursor)
{
	//TODO: Not supported yet
	_D("Skipping a link node\n");
}

bool ctx::access_config_loader::load()
{
	xmlDocPtr doc = xmlParseFile(CONFIG_PATH);
	IF_FAIL_RETURN_TAG(doc, false, _E, "XML parsing failed");

	xmlNodePtr cursor = xmlDocGetRootElement(doc);
	IF_FAIL_CATCH_TAG(cursor, _E, "No root node");
	IF_FAIL_CATCH_TAG(IS_NODE_OF(cursor, ELM_ROOT), _E, "Invalid root node");

	cursor = cursor->xmlChildrenNode;

	while (cursor) {
		if (IS_NODE_OF(cursor, ELM_PRIV)) {
			parse_priv_node(cursor);
		} else if (IS_NODE_OF(cursor, ELM_LINK)) {
			parse_link_node(cursor);
		}

		cursor = cursor->next;
	}

	xmlFreeDoc(doc);
	return true;

CATCH:
	xmlFreeDoc(doc);
	return false;
}
