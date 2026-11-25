#pragma once

#include <string>
#include <functional>

#include <common/src/error.h>
#include <common/src/uri.h>
#include <webextensions/webview_factory_host.h>

namespace orion {

// Interface
class KagiService {
public:
    enum class OnboardingStatus : int32_t {
        none,
        pending,
        in_progress,
        complete,
        error,
        not_applicable,
    };

    virtual ~KagiService() = default;
    static std::shared_ptr<KagiService> create(
        bridge::ProfileWorld& profile_world);
    virtual void get_or_create_kagi_client_id(std::function<void(std::string, orion::Error)> cb) = 0;
    virtual bool is_kagi_search_query_url(const orion::Uri& uri) = 0;
    virtual void try_kagi_search_onboarding(std::function<void(Error)> cb) = 0;
    virtual void set_onboarding_status(OnboardingStatus status) = 0;
    virtual OnboardingStatus onboarding_status() const = 0;
    virtual void cancel_all_operations(std::function<void()> cancelled_cb) = 0;
};

} // namespace orion
