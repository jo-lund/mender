// Copyright 2023 Northern.tech AS
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#ifndef MENDER_API_AUTH_HPP
#define MENDER_API_AUTH_HPP

#include <functional>
#include <string>
#include <vector>

#include <common/conf.hpp>
#include <common/crypto.hpp>
#include <common/dbus.hpp>
#include <common/error.hpp>
#include <common/events.hpp>
#include <common/expected.hpp>
#include <common/http.hpp>
#include <common/optional.hpp>

namespace mender {
namespace api {
namespace auth {

using namespace std;

namespace crypto = mender::common::crypto;
namespace dbus = mender::common::dbus;
namespace error = mender::common::error;
namespace events = mender::common::events;
namespace expected = mender::common::expected;

enum AuthClientErrorCode {
	NoError = 0,
	SetupError,
	RequestError,
	ResponseError,
	APIError,
	UnauthorizedError,
	AuthenticationError,
};

class AuthClientErrorCategoryClass : public std::error_category {
public:
	const char *name() const noexcept override;
	string message(int code) const override;
};
extern const AuthClientErrorCategoryClass AuthClientErrorCategory;

error::Error MakeError(AuthClientErrorCode code, const string &msg);

struct AuthData {
	string server_url;
	string token;
};
using ExpectedAuthData = expected::expected<AuthData, error::Error>;

using APIResponse = ExpectedAuthData;
using APIResponseHandler = function<void(APIResponse)>;

using AuthenticatedAction = function<void(ExpectedAuthData)>;
using ReAuthenticatedAction = function<void()>;

error::Error FetchJWTToken(
	mender::http::Client &client,
	const vector<string> &servers,
	const crypto::Args &args,
	const string &device_identity_script_path,
	APIResponseHandler api_handler,
	const string &tenant_token = "");

class Authenticator {
public:
	Authenticator(events::EventLoop &loop, chrono::seconds auth_timeout = chrono::minutes {1}) :
		loop_ {loop},
		dbus_client_ {loop},
		auth_timeout_ {auth_timeout},
		auth_timeout_timer_ {loop} {};

	void ExpireToken();
	error::Error WithToken(AuthenticatedAction action);

	// Register a callback to be called on re-authentication. Will overwrite the
	// stored callback with the new one.
	void RegisterTokenReceivedCallback(ReAuthenticatedAction action) {
		action_ = action;
	}

private:
	void PostPendingActions(ExpectedAuthData &ex_auth_data);
	error::Error StartWatchingTokenSignal();
	error::Error RequestNewToken(optional<AuthenticatedAction> opt_action);

	bool token_fetch_in_progress_ = false;
	events::EventLoop &loop_;
	dbus::DBusClient dbus_client_;
	chrono::seconds auth_timeout_;
	events::Timer auth_timeout_timer_;
	vector<AuthenticatedAction> pending_actions_;
	ReAuthenticatedAction action_ {nullptr};
	bool watching_token_signal_ {false};
};

} // namespace auth
} // namespace api
} // namespace mender

#endif // MENDER_API_AUTH_HPP
