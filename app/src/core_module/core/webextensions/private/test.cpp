#include <core/webextensions/private/test.h>
#include <common/src/log.h>
#include <core/webextensions/extension.h>
#include "test_extern_js.h"

#include <algorithm>

namespace WebExtensions {

namespace {

const char*
message_or_default(const std::string* message)
{
    return message && !message->empty() ? message->c_str() : "(no message)";
}

} // namespace

std::string
TestImpl::script_source_for_extension(WebExtension* extension)
{
    if (!extension)
        return "";

    const auto permissions = extension->permissions();
    if (std::find(permissions.begin(), permissions.end(), "debug") == permissions.end())
        return "";
    return test_extern_js;
}

void
TestImpl::sendMessage(
    v2::Promise<void> promise,
    const rfl::Generic* message,
    const rfl::Generic* argument)
{
    const rfl::Generic empty = rfl::Generic::Null;
    onMessage.notify_extension(
        promise.extension_id(),
        message ? *message : empty,
        argument ? *argument : empty);
    promise.resolve();
}

void
TestImpl::__notifyPass(v2::Promise<void> promise, const std::string* message)
{
    orion::log_info(
        "WebExtensionTest> PASS {%s}: %s",
        promise.extension_id().c_str(), message_or_default(message));
    promise.resolve();
}

void
TestImpl::__notifyFail(v2::Promise<void> promise, const std::string* message)
{
    orion::log_error(
        "WebExtensionTest> FAIL {%s}: %s",
        promise.extension_id().c_str(), message_or_default(message));
    promise.resolve();
}

void
TestImpl::__log(v2::Promise<void> promise, const std::string& message)
{
    orion::log_info(
        "WebExtensionTest> LOG {%s}: %s",
        promise.extension_id().c_str(), message.c_str());
    promise.resolve();
}

} // namespace WebExtensions
