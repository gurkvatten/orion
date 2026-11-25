#include <common/src/dispatch.h>
#include <core/webextensions/repository.h>
#include <common/src/memory.h>

#include <common/src/zipper.h>

#include <algorithm>

using namespace orion;

namespace WebExtensions {

class WebExtensionResourcesImpl : public WebExtensionResources {
public:
    WebExtensionResourcesImpl(const ExtensionSku& sku, const ExtensionType& type, const std::string& filename);
    ~WebExtensionResourcesImpl();
    const Manifest& manifest() const override { return m_manifest; }
    const std::string& scheme() const { return m_scheme; }
    std::optional<std::string> background_page_path() const override;
    bool background_page_is_service_worker() const override { return m_background_page_is_service_worker; }
    std::optional<WebExtensionResourceData> data_for_path(const std::string& path) const override;
private:
    void load_background_page();
    std::vector<std::string> background_script_paths() const;

    Manifest m_manifest;
    std::string m_scheme;
    ExtensionSku m_sku;
    Zipper::Node m_data_root;
    std::optional<std::string> m_background_page_path;
    bool m_background_page_is_service_worker = false;
    std::string m_generated_background_page_content;
    std::map<std::string, WebExtensionResourceData> m_data;
public:
    static std::shared_ptr<WebExtensionResources> create(const ExtensionSku& sku, const ExtensionType& type, const std::string& filename);
};

static std::map<std::string, WebExtensionResourceData>
flatten_node(const std::string& path, Zipper::Node* node)
{
    std::map<std::string, WebExtensionResourceData> result;
    for(const auto& [child_path, child_node] : node->children) {
        if(!child_node->is_dir) {
            result[path + "/" + child_path] = { child_node->file_data.data(), child_node->file_data.size() };
            continue;
        }
        for(const auto& [full_path, data] : flatten_node(path + "/" + child_path, child_node.get())) {
            result[full_path] = data;
        }
    }
    return result;
}

static std::optional<std::string>
sanitize_path(const std::string& raw_path)
{
    auto percent_decode = [](std::string_view input) -> std::optional<std::string> {
        std::string output;
        output.reserve(input.size());
        for (std::size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '%') {
                if (i + 2 >= input.size()
                    || !std::isxdigit(static_cast<unsigned char>(input[i + 1]))
                    || !std::isxdigit(static_cast<unsigned char>(input[i + 2]))) {
                    return std::nullopt; // malformed escape
                }
                const auto hex = [](unsigned char c) {
                    c = std::toupper(c);
                    return c <= '9' ? c - '0' : c - 'A' + 10;
                };
                output.push_back(static_cast<char>((hex(input[i + 1]) << 4) | hex(input[i + 2])));
                i += 2;
            } else {
                output.push_back(input[i]);
            }
        }
        return output;
    };

    auto decoded = percent_decode(raw_path);
    if (!decoded) {
        return std::nullopt;
    }

    std::vector<std::string> segments;
    std::size_t start = 0;
    while (start <= decoded->size()) {
        const auto end = decoded->find('/', start);
        const auto length = (end == std::string::npos ? decoded->size() : end) - start;
        std::string segment = decoded->substr(start, length);
        if (segment == "..") {
            if (segments.empty()) {
                return std::nullopt; // path escapes root
            }
            segments.pop_back();
        } else if (!segment.empty() && segment != ".") {
            segments.emplace_back(segment);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }

    std::string normalized = "/";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i) normalized.push_back('/');
        normalized.append(segments[i]);
    }
    return normalized;
}

static constexpr const char* generated_background_page_path = "/_orion_generated_background_page.html";

static constexpr const char* service_worker_emulation_bootstrap = R"html(
<script>
(() => {
    const global = globalThis;
    Object.defineProperty(global, "__orionServiceWorkerEmulation", {
        configurable: false,
        enumerable: false,
        value: true,
        writable: false
    });

    const workerScriptURL = () => {
        const script = document.getElementById("__orion_service_worker");
        return script && script.src ? script.src : location.href;
    };

    const markAsEmulatedScope = (constructor) => {
        if(typeof constructor !== "function") return false;
        try {
            Object.defineProperty(constructor, Symbol.hasInstance, {
                configurable: true,
                value: (candidate) => candidate === global
            });
            return global instanceof constructor;
        } catch(error) {
            return false;
        }
    };

    const defineScopeConstructor = (name) => {
        if(markAsEmulatedScope(global[name])) return;
        const constructor = function() {};
        Object.defineProperty(constructor, "name", {value: name});
        Object.defineProperty(constructor, Symbol.hasInstance, {
            value: (candidate) => candidate === global
        });
        try {
            Object.defineProperty(global, name, {
                configurable: true,
                value: constructor
            });
        } catch(error) {
            console.debug(`Unable to emulate ${name}`, error);
        }
    };

    defineScopeConstructor("WorkerGlobalScope");
    defineScopeConstructor("ServiceWorkerGlobalScope");

    if(typeof global.importScripts !== "function") {
        global.importScripts = (...urls) => {
            for(const url of urls) {
                const resolved = new URL(String(url), workerScriptURL()).href;
                const request = new XMLHttpRequest();
                request.open("GET", resolved, false);
                request.send();
                if(request.status !== 0 && (request.status < 200 || request.status >= 300)) {
                    throw new DOMException(
                        `Unable to import service-worker script ${resolved}`,
                        "NetworkError"
                    );
                }
                (0, eval)(`${request.responseText}\n//# sourceURL=${resolved}`);
            }
        };
    }

    if(!global.clients) {
        global.clients = Object.freeze({
            claim: () => Promise.resolve(),
            get: () => Promise.resolve(undefined),
            matchAll: () => Promise.resolve([]),
            openWindow: () => Promise.resolve(null)
        });
    }
    if(!global.registration) {
        global.registration = Object.freeze({
            get active() {
                return Object.freeze({scriptURL: workerScriptURL(), state: "activated"});
            },
            get scope() {
                return new URL(".", workerScriptURL()).href;
            },
            unregister: () => Promise.resolve(false),
            update: () => Promise.resolve()
        });
    }
    if(!("serviceWorker" in global)) {
        Object.defineProperty(global, "serviceWorker", {
            configurable: true,
            enumerable: false,
            get: () => global.registration.active
        });
    }
    if(typeof global.skipWaiting !== "function") {
        global.skipWaiting = () => Promise.resolve();
    }
})();
</script>
)html";

static std::string
escape_html_attribute(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for(char c : value) {
        switch(c) {
            case '&': result += "&amp;"; break;
            case '"': result += "&quot;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::shared_ptr<WebExtensionResources>
WebExtensionResourcesImpl::create(const ExtensionSku& sku, const ExtensionType& type, const std::string& filename)
{
    return std::make_shared<WebExtensionResourcesImpl>(sku, type, filename);
}

WebExtensionResourcesImpl::WebExtensionResourcesImpl(
    const ExtensionSku& sku, const ExtensionType& type, const std::string& filename
) : m_sku(sku)
{
    log_debug("WebExtensionResources> Loading sku {%s}", m_sku.value.c_str());
    std::shared_ptr<Zipper> zipper = Zipper::create();
    Error err = zipper->extract_to_memory(filename, &m_data_root);
    if(err) {
        return;
    }
    m_data = flatten_node("", &m_data_root);
    log_debug("WebExtensionResources> Loaded {%s} <- %s", m_sku.value.c_str(), filename.c_str());
    auto manifest_data = data_for_path("/manifest.json");
    if(manifest_data) {
        std::string manifest_str((const char*)manifest_data->data, manifest_data->datalen);
        //printf("%s\n", manifest_str.c_str());
        std::string error;
        auto parsed = deserialize_json<Manifest>(manifest_str, &error);
        if(!parsed) {
            log_debug("WebExtensionResources> Failed to parse manifest.json: %s", error.c_str());
            m_manifest = Manifest();
        } else {
            m_manifest = std::move(*parsed);
        }
    }
    load_background_page();
    switch(type) {
        case ExtensionType::chrome: m_scheme = "chrome-extension"; break;
        case ExtensionType::firefox: m_scheme = "moz-extension"; break;
        default: m_scheme = "unsupported-extension"; break;
    }
}

std::optional<WebExtensionResourceData>
WebExtensionResourcesImpl::data_for_path(const std::string& raw_path) const
{
    std::optional<std::string> path = sanitize_path(raw_path);
    if(!path) {
        return std::nullopt;
    }
    auto it = m_data.find(path.value());
    if(it == m_data.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::string>
WebExtensionResourcesImpl::background_script_paths() const
{
    std::vector<std::string> result;
    ManifestBackground background = m_manifest.background.value_or(ManifestBackground());
    for(const std::string& raw_path : background.scripts.value_or(std::vector<std::string>())) {
        std::optional<std::string> path = sanitize_path(raw_path);
        if(path && *path != "/") {
            result.push_back(std::move(*path));
        }
    }
    return result;
}

void
WebExtensionResourcesImpl::load_background_page()
{
    ManifestBackground background = m_manifest.background.value_or(ManifestBackground());
    const bool has_page = background.page && !background.page->empty();
    const bool has_scripts = background.scripts && std::any_of(
        background.scripts->begin(), background.scripts->end(),
        [](const std::string& path) { return !path.empty(); });
    const bool has_service_worker =
        background.service_worker && !background.service_worker->empty();
    const bool use_service_worker = has_service_worker
        && (m_manifest.manifest_version.value_or(2) >= 3 || (!has_page && !has_scripts));

    if(has_page && !use_service_worker) {
        m_background_page_path = sanitize_path(*background.page);
        return;
    }

    std::vector<std::string> script_paths;
    if(use_service_worker) {
        if(auto path = sanitize_path(*background.service_worker); path && *path != "/") {
            script_paths.push_back(std::move(*path));
        }
    } else {
        script_paths = background_script_paths();
    }
    if(script_paths.empty()) {
        return;
    }

    m_background_page_is_service_worker = use_service_worker;
    m_background_page_path = generated_background_page_path;
    m_generated_background_page_content = "<!doctype html><meta charset=\"utf-8\">";
    if(use_service_worker) {
        m_generated_background_page_content += service_worker_emulation_bootstrap;
    }
    const bool use_modules = background.type && *background.type == "module";
    for(const std::string& script_path : script_paths) {
        m_generated_background_page_content +=
            "<script" + std::string(use_modules ? " type=\"module\"" : "")
            + (use_service_worker ? " id=\"__orion_service_worker\"" : "")
            + " src=\"" + escape_html_attribute(script_path) + "\"></script>";
    }
    m_data[generated_background_page_path] = WebExtensionResourceData {
        reinterpret_cast<uint8_t*>(m_generated_background_page_content.data()),
        m_generated_background_page_content.size(),
    };
}

std::optional<std::string>
WebExtensionResourcesImpl::background_page_path() const
{
    return m_background_page_path;
}

WebExtensionResourcesImpl::~WebExtensionResourcesImpl()
{
    log_debug("WebExtensionResources> Unloaded sku {%s}", m_sku.value.c_str());
}

class WebExtensionsRepositoryImpl : public WebExtensionsRepository, public std::enable_shared_from_this<WebExtensionsRepositoryImpl> {
public:
    WebExtensionsRepositoryImpl(CoreApi* api) : m_api(api) {}
    void get_extension_resources(const ExtensionDto& extension, WebExtensionResourcesCompletionHandler completion_handler) override;
private:
    CoreApi* m_api;
    std::map<ExtensionSku, std::weak_ptr<WebExtensionResources>> m_sku_to_repo;
    std::map<ExtensionSku, std::vector<WebExtensionResourcesCompletionHandler>> m_pending_sku_repo;
};

std::shared_ptr<WebExtensionsRepository>
WebExtensionsRepository::create(CoreApi* api) {
    return std::make_shared<WebExtensionsRepositoryImpl>(api);
}

void
WebExtensionsRepositoryImpl::get_extension_resources(
    const ExtensionDto& extension, WebExtensionResourcesCompletionHandler completion_handler)
{
    ExtensionSku sku = extension.sku;
    ExtensionType type = extension.type;
    auto it = m_sku_to_repo.find(sku);
    if(it != m_sku_to_repo.end()) {
        auto repo = it->second.lock();
        if(repo) {
            dispatch(on_main_thread(), [completion_handler=std::move(completion_handler),repo](){
                completion_handler(repo);
            });
            return;
        }
    }
    auto pending = m_pending_sku_repo.find(sku);
    if(pending != m_pending_sku_repo.end()) {
        pending->second.emplace_back(std::move(completion_handler));
        return;
    }
    m_pending_sku_repo[sku].emplace_back(std::move(completion_handler));
    dispatch(on_disk_thread(), [this,guard=shared_from_this(),sku,type,filename=extension.file_path](){
        std::shared_ptr<WebExtensionResources> resources = WebExtensionResourcesImpl::create(sku, type, filename);
        dispatch(on_main_thread(), [this,guard,sku,resources=std::move(resources)](){
            m_sku_to_repo[sku] = resources;
            std::vector<WebExtensionResourcesCompletionHandler> handlers = std::move(m_pending_sku_repo[sku]);
            m_pending_sku_repo.erase(sku);
            for(auto completion_handler : handlers) {
                completion_handler(resources);
            }
        });
    });
}

} // namespace WebExtensions
