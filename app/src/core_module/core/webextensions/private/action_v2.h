#pragma once

#include <core/core_api.h>

namespace WebExtensions {

class ActionV2Impl : public v2::Action {
public:
    void setBadgeBackgroundColor(
        v2::Promise<void> promise,
        const v2::ActionBadgeBackgroundColorDetails& details) override;
    void setBadgeText(
        v2::Promise<void> promise,
        const v2::ActionBadgeTextDetails& details) override;
    void setIcon(
        v2::Promise<void> promise,
        const v2::ActionSetIconDetails& details) override;
};

} // namespace WebExtensions
