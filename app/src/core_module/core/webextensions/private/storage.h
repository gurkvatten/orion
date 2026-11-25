#pragma once

#include <chrono>
#include <deque>
#include <map>

#include <common/src/coroutine.h>
#include <core/core_api.h>

namespace WebExtensions {

class WebExtension;
class WebExtensionsRuntimeImpl;

class StorageV2Impl : public v2::Storage {
public:
    static std::string script_source_for_extension(WebExtension* extension);
    std::optional<std::string> consume_sync_write_quota(
        const std::string& extension_id);
    void notify_changes(
        const std::string& extension_id,
        const std::string& area_name,
        const rfl::Generic& changes,
        bool allow_untrusted_contexts);

private:
    std::map<std::string, std::deque<std::chrono::steady_clock::time_point>>
        m_sync_writes;
};

class StorageAreaLocalV2Detail {
public:
    static constexpr bool is_read_only = false;
    static constexpr bool allow_untrusted_contexts_by_default = true;
    static constexpr bool honors_unlimited_storage = true;
    static constexpr bool has_write_rate_quota = false;
    static constexpr uint64_t quota_bytes = 10485760;
    static constexpr uint64_t quota_bytes_per_item = 0;
    static constexpr uint64_t max_items = 0;
    static constexpr const char* storage_id = "local";
};

class StorageAreaSyncV2Detail {
public:
    static constexpr bool is_read_only = false;
    static constexpr bool allow_untrusted_contexts_by_default = true;
    static constexpr bool honors_unlimited_storage = false;
    static constexpr bool has_write_rate_quota = true;
    static constexpr uint64_t quota_bytes = 102400;
    static constexpr uint64_t quota_bytes_per_item = 8192;
    static constexpr uint64_t max_items = 512;
    static constexpr const char* storage_id = "sync";
};

class StorageAreaManagedV2Detail {
public:
    static constexpr bool is_read_only = true;
    static constexpr bool allow_untrusted_contexts_by_default = true;
    static constexpr bool honors_unlimited_storage = false;
    static constexpr bool has_write_rate_quota = false;
    static constexpr uint64_t quota_bytes = 0;
    static constexpr uint64_t quota_bytes_per_item = 0;
    static constexpr uint64_t max_items = 0;
    static constexpr const char* storage_id = "managed";
};

class StorageAreaSessionV2Detail {
public:
    static constexpr bool is_read_only = false;
    static constexpr bool allow_untrusted_contexts_by_default = false;
    static constexpr bool honors_unlimited_storage = false;
    static constexpr bool has_write_rate_quota = false;
    static constexpr uint64_t quota_bytes = 10485760;
    static constexpr uint64_t quota_bytes_per_item = 0;
    static constexpr uint64_t max_items = 0;
    static constexpr const char* storage_id = "session";
};

template<typename Interface, typename Detail>
class StorageAreaV2 : public Interface, public Detail {
public:
    using Interface::onChanged;

    void set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world);
    void set_storage(StorageV2Impl* storage);

    void get(v2::Promise<rfl::Generic> promise, const rfl::Generic* keys) override;
    void getKeys(v2::Promise<std::vector<std::string>> promise) override;
    void getBytesInUse(
        v2::Promise<double> promise, const rfl::Generic* keys) override;
    void set(v2::Promise<void> promise, const rfl::Generic& items) override;
    void remove(v2::Promise<void> promise, const rfl::Generic& keys) override;
    void clear(v2::Promise<void> promise) override;
    void setAccessLevel(
        v2::Promise<void> promise,
        const v2::StorageAccessLevelOptions& access_options) override;

private:
    using GetQuery = std::map<std::string, std::optional<std::string>>;

    detached_task get(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        v2::Promise<rfl::Generic> promise,
        std::optional<GetQuery> keys);
    detached_task getKeys(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        v2::Promise<std::vector<std::string>> promise);
    detached_task getBytesInUse(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        v2::Promise<double> promise,
        std::optional<std::vector<std::string>> keys);
    detached_task set(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        v2::Promise<void> promise,
        std::map<std::string, std::string> items);
    detached_task remove(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        v2::Promise<void> promise,
        std::vector<std::string> keys);
    detached_task clear(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        v2::Promise<void> promise);
    detached_task setAccessLevel(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        v2::Promise<void> promise,
        bool allow_untrusted_contexts);
    void notify_changes(
        const std::string& extension_id,
        const rfl::Generic& changes,
        bool allow_untrusted_contexts);
    static async_task<std::optional<bool>> allow_untrusted_contexts(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        const std::string& extension_id);
    bool check_access(
        v2::SecurityProfile security_profile,
        bool allow_untrusted_contexts) const;
    std::string access_error() const;

    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
    StorageV2Impl* m_storage = nullptr;
};

using StorageAreaLocalV2Impl =
    StorageAreaV2<v2::StorageAreaLocal, StorageAreaLocalV2Detail>;
using StorageAreaSyncV2Impl =
    StorageAreaV2<v2::StorageAreaSync, StorageAreaSyncV2Detail>;
using StorageAreaManagedV2Impl =
    StorageAreaV2<v2::StorageAreaManaged, StorageAreaManagedV2Detail>;
using StorageAreaSessionV2Impl =
    StorageAreaV2<v2::StorageAreaSession, StorageAreaSessionV2Detail>;

} // namespace WebExtensions
