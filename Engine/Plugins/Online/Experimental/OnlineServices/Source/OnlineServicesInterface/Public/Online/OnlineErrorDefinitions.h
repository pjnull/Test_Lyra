// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#define LOCTEXT_NAMESPACE "OSSErrors"

#include "Online/OnlineError.h"

namespace UE::Online::Errors {

ONLINE_ERROR_CATEGORY(OnlineServices, Engine, 0x1, "Online Services")

ONLINE_ERROR(OnlineServices, NoConnection, 1, TEXT("no_connection"), LOCTEXT("NotConnected", "No valid connection"))
ONLINE_ERROR(OnlineServices, RequestFailure, 2, TEXT("request_failure"), LOCTEXT("RequestFailure", "Failed to send request"))
ONLINE_ERROR(OnlineServices, InvalidCreds, 3, TEXT("invalid_creds"), LOCTEXT("InvalidCreds", "Invalid credentials"))
ONLINE_ERROR(OnlineServices, InvalidUser, 4, TEXT("invalid_user"), LOCTEXT("InvalidUser", "No valid user"))
ONLINE_ERROR(OnlineServices, InvalidAuth, 5, TEXT("invalid_auth"), LOCTEXT("InvalidAuth", "No valid auth"))
ONLINE_ERROR(OnlineServices, AccessDenied, 6, TEXT("access_denied"), LOCTEXT("AccessDenied", "Access denied"))
ONLINE_ERROR(OnlineServices, TooManyRequests, 7, TEXT("too_many_requests"), LOCTEXT("TooManyRequests", "Too many requests"))
ONLINE_ERROR(OnlineServices, AlreadyPending, 8, TEXT("already_pending"), LOCTEXT("AlreadyPending", "Request already pending"))
ONLINE_ERROR(OnlineServices, InvalidParams, 9, TEXT("invalid_params"), LOCTEXT("InvalidParams", "Invalid params specified"))
ONLINE_ERROR(OnlineServices, CantParse, 10, TEXT("cant_parse"), LOCTEXT("CantParse", "Cannot parse results"))
ONLINE_ERROR(OnlineServices, InvalidResults, 11, TEXT("invalid_results"), LOCTEXT("InvalidResults", "Results were invalid"))
ONLINE_ERROR(OnlineServices, IncompatibleVersion, 12, TEXT("incompatible_version"), LOCTEXT("IncompatibleVersion", "Incompatible client version"))
ONLINE_ERROR(OnlineServices, NotConfigured, 13, TEXT("not_configured"), LOCTEXT("NotConfigured", "No valid configuration"))
ONLINE_ERROR(OnlineServices, NotImplemented, 14, TEXT("not_implemented"), LOCTEXT("NotImplemented", "Not implemented"))
ONLINE_ERROR(OnlineServices, MissingInterface, 15, TEXT("missing_interface"), LOCTEXT("MissingInterface", "Interface not found"))
ONLINE_ERROR(OnlineServices, Canceled, 16, TEXT("canceled"), LOCTEXT("Canceled", "Operation was canceled"))

} /* namespace UE::Online::Errors */

#undef LOCTEXT_NAMESPACE