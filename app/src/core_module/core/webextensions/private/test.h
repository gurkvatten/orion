#pragma once

#include <core/core_api.h>

namespace WebExtensions {

class TestImpl : public v2::Test {
public:
    static std::string script_source_for_extension(WebExtension* extension);

    void sendMessage(
        v2::Promise<void> promise,
        const rfl::Generic* message,
        const rfl::Generic* argument) override;
    void __notifyPass(
        v2::Promise<void> promise,
        const std::string* message) override;
    void __notifyFail(
        v2::Promise<void> promise,
        const std::string* message) override;
    void __log(
        v2::Promise<void> promise,
        const std::string& message) override;
};

} // namespace WebExtensions
