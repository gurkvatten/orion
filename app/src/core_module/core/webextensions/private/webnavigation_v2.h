#pragma once

#include <core/core_api.h>

namespace WebExtensions {

class WebNavigationV2Impl : public v2::WebNavigationV2 {
public:
    void getFrame(
        v2::Promise<v2::WebNavigationFrameDetails> promise,
        const v2::WebNavigationGetFrameDetails& details) override;
    void getAllFrames(
        v2::Promise<std::vector<v2::WebNavigationFrameDetails>> promise,
        const v2::WebNavigationGetAllFramesDetails& details) override;
};

} // namespace WebExtensions
