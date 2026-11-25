#include <core/webextensions/private/action_v2.h>

namespace WebExtensions {

void
ActionV2Impl::setBadgeBackgroundColor(
    v2::Promise<void> promise,
    const v2::ActionBadgeBackgroundColorDetails&)
{
    promise.resolve();
}

void
ActionV2Impl::setBadgeText(
    v2::Promise<void> promise,
    const v2::ActionBadgeTextDetails&)
{
    promise.resolve();
}

void
ActionV2Impl::setIcon(
    v2::Promise<void> promise,
    const v2::ActionSetIconDetails&)
{
    promise.resolve();
}

} // namespace WebExtensions
