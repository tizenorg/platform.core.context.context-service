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

#ifndef _CONTEXT_PEER_CREDENTIALS_H_
#define _CONTEXT_PEER_CREDENTIALS_H_

#include <sys/types.h>
#include <gio/gio.h>
#include <string>

namespace ctx {

	class Credentials {
	public:
		char *packageId;
		char *client;	/* default: smack label */
		char *session;
		char *user;		/* default: UID */
		Credentials(char *pkgId, char *cli, char *sess, char *usr);
		~Credentials();
	};

	namespace peer_creds {

		bool get(GDBusConnection *connection, const char *uniqueName, const char *cookie, Credentials **creds);

	}	/* namespace peer_creds */
}	/* namespace ctx */

#endif	/* End of _CONTEXT_PEER_CREDENTIALS_H_ */
