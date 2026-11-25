#include <core/webextensions/private/webnavigation_v2.h>

namespace WebExtensions {

void
WebNavigationV2Impl::getFrame(
    v2::Promise<v2::WebNavigationFrameDetails> promise,
    const v2::WebNavigationGetFrameDetails&)
{
    promise.resolve();
}

void
WebNavigationV2Impl::getAllFrames(
    v2::Promise<std::vector<v2::WebNavigationFrameDetails>> promise,
    const v2::WebNavigationGetAllFramesDetails&)
{
    promise.resolve(std::vector<v2::WebNavigationFrameDetails>{});
}

} // namespace WebExtensions
