#pragma once

#include <vector>
#include <memory>
#include <functional>

#include <core/dto/extension_dto.h>
#include <core/webextensions/runtime.h>
#include <core/webkit_bridge.h>
#include <core/private_mode.h>
#include <common/src/observable.h>
#include <common/src/func_types.h>

#include <glib.h>
#include <util/stop_cancellable.h>
#include <gtk/gtk.h>

#include <util/ref_util.h>

class OpaqueWebView;
class OpaqueNetworkSession;
class OpaqueWebKitStore;

void web_view_unref(OpaqueWebView* web_view);

struct WebViewDeleter {
    void operator()(OpaqueWebView* web_view) const {
        web_view_unref(web_view);
    }
};

typedef std::unique_ptr<OpaqueWebView, WebViewDeleter> WebViewRef;

namespace WebExtensions {

struct WebExtensionBackgroundPagesChange;
struct WebExtensionSchemeRegistration;

extern std::atomic<int> global_webview_factory_count;

struct WebViewMapChange {
    std::vector<orion::gtk::WeakRefWrapper> created{};
    std::vector<orion::gtk::WeakRefWrapper> deleted{};
};

class ContentFilterStore {
public:
    OpaqueWebKitStore* webkit_store() const { return m_webkit_store; }
    void set_webkit_store(OpaqueWebKitStore* store) { m_webkit_store = store; }
    std::shared_ptr<orion::StopCancellable> stop_cancellable() { return m_stop_cancellable; }
    void set_stop_cancellable(std::shared_ptr<orion::StopCancellable> cancellable) { m_stop_cancellable = std::move(cancellable); }
    int32_t pending_ops() const { return m_pending_ops; }
    void add_pending_ops() { m_pending_ops++; }
    void remove_pending_ops() { m_pending_ops--; }
    bool is_cancelling() const { return m_is_cancelling; }
    void set_is_cancelling(bool value) { m_is_cancelling = value; }
    const std::vector<orion::Fn>& cancelled_cbs() const { return m_cancelled_cbs; }
    void add_cancelled_cb(orion::Fn cb) { m_cancelled_cbs.push_back(std::move(cb)); }
    void clear_cancelled_cbs() { m_cancelled_cbs.clear(); }

private:
    OpaqueWebKitStore* m_webkit_store{};
    std::shared_ptr<orion::StopCancellable> m_stop_cancellable{};
    int32_t m_pending_ops = 0;
    bool m_is_cancelling = false;
    std::vector<orion::Fn> m_cancelled_cbs{};
};

class WebExtensionWebViewFactory
    : public orion::bridge::WebViewFactoryHost,
      public std::enable_shared_from_this<WebExtensionWebViewFactory> {
public:
    explicit WebExtensionWebViewFactory(orion::bridge::WebViewFactoryBridge& bridge)
        : m_bridge(bridge.weak_from_this()) {}

    std::weak_ptr<orion::bridge::WebViewFactoryBridge> bridge() const { return m_bridge; }
    std::weak_ptr<orion::bridge::ProfileWorld> parent_world() const {
        auto bridge = m_bridge.lock();
        return bridge ? bridge->parent_world() : std::weak_ptr<orion::bridge::ProfileWorld>();
    }
    virtual ~WebExtensionWebViewFactory() {};

    typedef std::function<void(WebViewRef web_view)> WebViewReply;

    virtual void web_view_for_content(WebViewReply reply) = 0;

    virtual WebViewRef web_view_for_content_synchronous(WebViewRef related_webview) = 0;

    virtual void web_view_for_extension_popup(orion::ExtensionId extension_id, WebViewReply reply) = 0;

    virtual bool show_extension_background_web_inspector(orion::ExtensionId extension_id) = 0;

    virtual void update_background_pages(const WebExtensionBackgroundPagesChange& change) = 0;

    virtual void register_scheme(
        const WebExtensionSchemeRegistration& registration,
        WebExtensionSchemeRequestHandler handler
    ) = 0;

    virtual OpaqueNetworkSession* network_session() = 0;

    virtual std::vector<orion::gtk::WeakRefWrapper> web_views() = 0;

    static std::shared_ptr<WebExtensionWebViewFactory> create(
        orion::bridge::WebViewFactoryBridge& bridge);

    virtual orion::Observable<WebViewMapChange>& web_views_change() const = 0;

    virtual void set_web_views_observer(orion::ObserverId observer_id) = 0;

    virtual orion::ObserverId web_views_observer() const = 0;

    virtual ContentFilterStore* filter_store() = 0;

    orion::ProfileId profile_id() const {
        auto world = parent_world().lock();
        assert(world);
        return world->profile_id();
    }

    orion::PrivateMode private_mode() const {
        auto world = parent_world().lock();
        assert(world);
        return world->private_mode();
    }

    static std::vector<std::string> get_profile_path_parts(
        orion::ProfileId profile_id,
        orion::PrivateMode private_mode
    );
protected:
    void detach() { m_attachment.reset(); }
    void initialize_platform() override {}
    bool is_ready() const {
        auto bridge = m_bridge.lock();
        return bridge && bridge->is_ready();
    }
    void when_ready(std::function<void()> reply) {
        if(auto bridge = m_bridge.lock()) bridge->when_ready(std::move(reply));
    }
    void register_schemes() const {
        if(auto bridge = m_bridge.lock()) bridge->register_schemes();
    }
    std::weak_ptr<orion::bridge::WebViewBridge> create_web_view_bridge() {
        auto bridge = m_bridge.lock();
        return bridge ? bridge->create_web_view_bridge()
                      : std::weak_ptr<orion::bridge::WebViewBridge>();
    }
    void configure_web_view(orion::bridge::WebViewBridge& web_view) {
        if(auto bridge = m_bridge.lock()) bridge->configure_web_view(web_view);
    }
    std::optional<std::string> document_uri(
        orion::ExtensionId id, WebExtensions::WebExtensionDocumentKind kind) const {
        auto bridge = m_bridge.lock();
        return bridge ? bridge->document_uri(id, kind) : std::nullopt;
    }

private:
    std::weak_ptr<orion::bridge::WebViewFactoryBridge> m_bridge;
    orion::bridge::HostAttachment m_attachment;

};

} // namespace WebExtensions

using WebExtensions::WebExtensionWebViewFactory;
