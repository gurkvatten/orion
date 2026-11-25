#include <core/webextensions/private/storage.h>

#include <common/src/coroutine.h>
#include <common/src/dispatch.h>
#include <core/webextensions/runtime_impl.h>

#include <rfl/json.hpp>

#include <algorithm>

namespace WebExtensions {
namespace {

rfl::Generic
stored_value(const std::string& value)
{
    auto parsed = rfl::json::read<rfl::Generic>(value);
    return parsed ? std::move(*parsed) : rfl::Generic(value);
}

rfl::Generic
storage_change(const std::string* old_value, const std::string* new_value)
{
    rfl::Generic::Object change;
    if(old_value) {
        change["oldValue"] = stored_value(*old_value);
    }
    if(new_value) {
        change["newValue"] = stored_value(*new_value);
    }
    return rfl::Generic(std::move(change));
}

rfl::Generic::Object
storage_changes(const std::vector<orion::ExtensionStorageAreas::Change>& values)
{
    rfl::Generic::Object changes;
    for (const auto& value : values) {
        changes[value.key] = storage_change(
            value.old_value ? &*value.old_value : nullptr,
            value.new_value ? &*value.new_value : nullptr);
    }
    return changes;
}

std::vector<std::string>
storage_keys(const rfl::Generic& keys)
{
    if(const auto* key = std::get_if<std::string>(&keys.get())) {
        return {*key};
    }

    std::vector<std::string> result;
    const auto array = keys.to_array().value();
    for(const auto& item : array) {
        result.push_back(item.to_string().value());
    }
    return result;
}

bool
is_untrusted_context(v2::SecurityProfile security_profile)
{
    return security_profile == v2::SecurityProfile::ContentScript;
}

bool
extension_has_permission(
    orion::bridge::ProfileWorld& profile_world,
    const std::string& extension_id,
    const std::string& permission)
{
    auto* runtime = profile_world.runtime();
    if(!runtime) return false;
    for(const auto& weak_extension
        : runtime->get_extensions().value_or(WebExtensionList())) {
        const auto extension = weak_extension.lock();
        if(!extension || extension->extension_id().value != extension_id) {
            continue;
        }
        const auto permissions = extension->permissions();
        return std::find(permissions.begin(), permissions.end(), permission)
            != permissions.end();
    }
    return false;
}

} // namespace

std::string
StorageV2Impl::script_source_for_extension(WebExtension*)
{
    return R"v2storage(
(function(){
    const implementations = window.__webextensions_v2_implementations__ =
        window.__webextensions_v2_implementations__ || {};

    implementations["StorageAreaLocal.QUOTA_BYTES"] = () => 10485760;
    implementations["StorageAreaSync.QUOTA_BYTES"] = () => 102400;
    implementations["StorageAreaSync.QUOTA_BYTES_PER_ITEM"] = () => 8192;
    implementations["StorageAreaSync.MAX_ITEMS"] = () => 512;
    implementations["StorageAreaSync.MAX_WRITE_OPERATIONS_PER_HOUR"] = () => 1800;
    implementations["StorageAreaSync.MAX_WRITE_OPERATIONS_PER_MINUTE"] = () => 120;
    implementations["StorageAreaSync.MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE"] = () => 1000000;
    implementations["StorageAreaManaged.QUOTA_BYTES"] = () => 5242880;
    implementations["StorageAreaSession.QUOTA_BYTES"] = () => 10485760;
})();
)v2storage";
}

std::optional<std::string>
StorageV2Impl::consume_sync_write_quota(const std::string& extension_id)
{
    constexpr size_t writes_per_minute = 120;
    constexpr size_t writes_per_hour = 1800;

    const auto now = std::chrono::steady_clock::now();
    auto& writes = m_sync_writes[extension_id];
    while(!writes.empty() && now - writes.front() >= std::chrono::hours(1)) {
        writes.pop_front();
    }
    if(writes.size() >= writes_per_hour) {
        return "MAX_WRITE_OPERATIONS_PER_HOUR quota exceeded";
    }

    const auto minute_start = now - std::chrono::minutes(1);
    const auto first_minute_write = std::lower_bound(
        writes.begin(), writes.end(), minute_start);
    if(static_cast<size_t>(std::distance(first_minute_write, writes.end()))
        >= writes_per_minute) {
        return "MAX_WRITE_OPERATIONS_PER_MINUTE quota exceeded";
    }

    writes.push_back(now);
    return std::nullopt;
}

void
StorageV2Impl::notify_changes(
    const std::string& extension_id,
    const std::string& area_name,
    const rfl::Generic& changes,
    bool allow_untrusted_contexts)
{
    onChanged.notify_extension_if(
        extension_id,
        [allow_untrusted_contexts](auto&, const auto& registration) {
            return allow_untrusted_contexts
                || !is_untrusted_context(registration.security_profile);
        },
        changes, area_name);
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::set_profile_world(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
{
    m_profile_world = profile_world;
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::set_storage(StorageV2Impl* storage)
{
    m_storage = storage;
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::get(
    v2::Promise<rfl::Generic> promise, const rfl::Generic* keys)
{
    std::optional<GetQuery> query;
    if(keys) {
        query.emplace();
        if(const auto* key = std::get_if<std::string>(&keys->get())) {
            (*query)[*key] = std::nullopt;
        }
        else if(const auto* array = std::get_if<rfl::Generic::Array>(&keys->get())) {
            for(const auto& item : *array) {
                (*query)[item.to_string().value()] = std::nullopt;
            }
        }
        else {
            const auto object = keys->to_object().value();
            for(const auto& [key, default_value] : object) {
                (*query)[key] = rfl::json::write(default_value);
            }
        }
    }
    get(m_profile_world, std::move(promise), std::move(query));
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::getKeys(
    v2::Promise<std::vector<std::string>> promise)
{
    getKeys(m_profile_world, std::move(promise));
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::getBytesInUse(
    v2::Promise<double> promise, const rfl::Generic* keys)
{
    std::optional<std::vector<std::string>> parsed_keys;
    if (keys) {
        parsed_keys = storage_keys(*keys);
    }
    getBytesInUse(
        m_profile_world, std::move(promise), std::move(parsed_keys));
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::set(
    v2::Promise<void> promise, const rfl::Generic& items)
{
    std::map<std::string, std::string> serialized;
    const auto object = items.to_object().value();
    for(const auto& [key, value] : object) {
        serialized[key] = rfl::json::write(value);
    }
    set(m_profile_world, std::move(promise), std::move(serialized));
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::remove(
    v2::Promise<void> promise, const rfl::Generic& keys)
{
    remove(m_profile_world, std::move(promise), storage_keys(keys));
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::clear(v2::Promise<void> promise)
{
    clear(m_profile_world, std::move(promise));
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::setAccessLevel(
    v2::Promise<void> promise, const v2::StorageAccessLevelOptions& access_options)
{
    if(is_untrusted_context(promise.security_profile())) {
        promise.reject("setAccessLevel is restricted to trusted extension contexts");
        return;
    }
    setAccessLevel(
        m_profile_world, std::move(promise),
        access_options.accessLevel == "TRUSTED_AND_UNTRUSTED_CONTEXTS");
}

template<typename Interface, typename Detail>
detached_task
StorageAreaV2<Interface, Detail>::get(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    v2::Promise<rfl::Generic> promise,
    std::optional<GetQuery> keys)
{
    auto world = profile_world.lock();
    if(!world) co_return;
    world.reset();
    const auto allow_untrusted
        = co_await allow_untrusted_contexts(profile_world, promise.extension_id());
    world = profile_world.lock();
    if(!world) co_return;
    if(!allow_untrusted) {
        promise.reject();
        co_return;
    }
    if(!check_access(promise.security_profile(), *allow_untrusted)) {
        promise.reject(access_error());
        co_return;
    }

    std::vector<orion::ExtensionStorageAreas::KeyValue> values;
    if(!keys) {
        orion::ExtensionStorageAreas::GetAllQuery query;
        query.extension_id = promise.extension_id();
        query.storage_id = Detail::storage_id;
        auto* api = &world->core_api();
        world.reset();
        auto reply = co_await async(invoke(
            api,
            &orion::CoreApi::extension_storage_areas_get_all), query);
        world = profile_world.lock();
        if(!world) co_return;
        if(reply.err) {
            promise.reject();
            co_return;
        }
        values = std::move(reply.key_values);
    }
    else {
        if(keys->empty()) {
            promise.resolve(rfl::Generic(rfl::Generic::Object{}));
            co_return;
        }
        orion::ExtensionStorageAreas::GetQuery query;
        query.extension_id = promise.extension_id();
        query.storage_id = Detail::storage_id;
        for(const auto& [key, default_value] : *keys) {
            query.key_values.push_back({.key = key, .value = default_value});
        }
        auto* api = &world->core_api();
        world.reset();
        auto reply = co_await async(invoke(
            api,
            &orion::CoreApi::extension_storage_areas_get), query);
        world = profile_world.lock();
        if(!world) co_return;
        if(reply.err) {
            promise.reject();
            co_return;
        }
        values = std::move(reply.key_values);
    }

    rfl::Generic::Object result;
    for(const auto& item : values) {
        if(item.value) {
            result[item.key] = stored_value(*item.value);
        }
    }
    promise.resolve(rfl::Generic(std::move(result)));
}

template<typename Interface, typename Detail>
detached_task
StorageAreaV2<Interface, Detail>::getKeys(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    v2::Promise<std::vector<std::string>> promise)
{
    auto world = profile_world.lock();
    if(!world) co_return;
    world.reset();
    const auto allow_untrusted
        = co_await allow_untrusted_contexts(profile_world, promise.extension_id());
    world = profile_world.lock();
    if(!world) co_return;
    if(!allow_untrusted) {
        promise.reject();
        co_return;
    }
    if(!check_access(promise.security_profile(), *allow_untrusted)) {
        promise.reject(access_error());
        co_return;
    }
    orion::ExtensionStorageAreas::GetKeysQuery query;
    query.extension_id = promise.extension_id();
    query.storage_id = Detail::storage_id;
    auto* api = &world->core_api();
    world.reset();
    auto reply = co_await async(invoke(
        api,
        &orion::CoreApi::extension_storage_areas_get_keys), query);
    if(profile_world.expired()) co_return;
    if(reply.err) {
        promise.reject();
        co_return;
    }
    promise.resolve(reply.keys);
}

template<typename Interface, typename Detail>
detached_task
StorageAreaV2<Interface, Detail>::getBytesInUse(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    v2::Promise<double> promise,
    std::optional<std::vector<std::string>> keys)
{
    auto world = profile_world.lock();
    if(!world) co_return;
    world.reset();
    const auto allow_untrusted
        = co_await allow_untrusted_contexts(profile_world, promise.extension_id());
    world = profile_world.lock();
    if(!world) co_return;
    if(!allow_untrusted) {
        promise.reject();
        co_return;
    }
    if(!check_access(promise.security_profile(), *allow_untrusted)) {
        promise.reject(access_error());
        co_return;
    }
    orion::ExtensionStorageAreas::GetBytesInUseQuery query;
    query.extension_id = promise.extension_id();
    query.storage_id = Detail::storage_id;
    query.keys = std::move(keys);
    auto* api = &world->core_api();
    world.reset();
    auto reply = co_await async(invoke(
        api,
        &orion::CoreApi::extension_storage_areas_get_bytes_in_use), query);
    if(profile_world.expired()) co_return;
    if(reply.err) {
        promise.reject();
        co_return;
    }
    promise.resolve(static_cast<double>(reply.bytes_in_use));
}

template<typename Interface, typename Detail>
detached_task
StorageAreaV2<Interface, Detail>::set(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    v2::Promise<void> promise,
    std::map<std::string, std::string> items)
{
    auto world = profile_world.lock();
    if(!world) co_return;
    world.reset();
    const auto allow_untrusted
        = co_await allow_untrusted_contexts(profile_world, promise.extension_id());
    world = profile_world.lock();
    if(!world) co_return;
    if(!allow_untrusted) {
        promise.reject();
        co_return;
    }
    if(!check_access(promise.security_profile(), *allow_untrusted)) {
        promise.reject(access_error());
        co_return;
    }
    if constexpr (Detail::is_read_only) {
        promise.reject("This storage area is read-only");
        co_return;
    }
    if(items.empty()) {
        promise.resolve();
        co_return;
    }
    if constexpr (Detail::has_write_rate_quota) {
        if(const auto error = m_storage->consume_sync_write_quota(
            promise.extension_id())) {
            promise.reject(*error);
            co_return;
        }
    }

    orion::ExtensionStorageAreas::SetQuery query;
    query.extension_id = promise.extension_id();
    query.storage_id = Detail::storage_id;
    for(const auto& [key, value] : items) {
        query.key_values.push_back({.key = key, .value = value});
    }
    query.quota = orion::ExtensionStorageAreas::Quota{
        .bytes = Detail::quota_bytes,
        .bytes_per_item = Detail::quota_bytes_per_item
            ? std::optional<uint64_t>(Detail::quota_bytes_per_item)
            : std::nullopt,
        .max_items = Detail::max_items
            ? std::optional<uint64_t>(Detail::max_items)
            : std::nullopt,
    };
    if constexpr (Detail::honors_unlimited_storage) {
        if(extension_has_permission(
            *world, promise.extension_id(), "unlimitedStorage")) {
            query.quota->bytes.reset();
        }
    }
    auto* api = &world->core_api();
    world.reset();
    auto reply = co_await async(invoke(
        api,
        &orion::CoreApi::extension_storage_areas_set), query);
    world = profile_world.lock();
    if(!world) co_return;
    if(reply.err) {
        promise.reject(reply.err.message());
        co_return;
    }

    rfl::Generic::Object changes = storage_changes(reply.changes);
    if(!changes.empty()) {
        notify_changes(
            promise.extension_id(), rfl::Generic(std::move(changes)),
            *allow_untrusted);
    }
    promise.resolve();
}

template<typename Interface, typename Detail>
detached_task
StorageAreaV2<Interface, Detail>::remove(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    v2::Promise<void> promise,
    std::vector<std::string> keys)
{
    auto world = profile_world.lock();
    if(!world) co_return;
    world.reset();
    const auto allow_untrusted
        = co_await allow_untrusted_contexts(profile_world, promise.extension_id());
    world = profile_world.lock();
    if(!world) co_return;
    if(!allow_untrusted) {
        promise.reject();
        co_return;
    }
    if(!check_access(promise.security_profile(), *allow_untrusted)) {
        promise.reject(access_error());
        co_return;
    }
    if constexpr (Detail::is_read_only) {
        promise.reject("This storage area is read-only");
        co_return;
    }
    if(keys.empty()) {
        promise.resolve();
        co_return;
    }
    if constexpr (Detail::has_write_rate_quota) {
        if(const auto error = m_storage->consume_sync_write_quota(
            promise.extension_id())) {
            promise.reject(*error);
            co_return;
        }
    }

    orion::ExtensionStorageAreas::RemoveQuery query;
    query.extension_id = promise.extension_id();
    query.storage_id = Detail::storage_id;
    query.keys = keys;
    auto* api = &world->core_api();
    world.reset();
    auto reply = co_await async(invoke(
        api,
        &orion::CoreApi::extension_storage_areas_remove), query);
    world = profile_world.lock();
    if(!world) co_return;
    if(reply.err) {
        promise.reject();
        co_return;
    }

    rfl::Generic::Object changes = storage_changes(reply.changes);
    if(!changes.empty()) {
        notify_changes(
            promise.extension_id(), rfl::Generic(std::move(changes)),
            *allow_untrusted);
    }
    promise.resolve();
}

template<typename Interface, typename Detail>
detached_task
StorageAreaV2<Interface, Detail>::clear(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    v2::Promise<void> promise)
{
    auto world = profile_world.lock();
    if(!world) co_return;
    world.reset();
    const auto allow_untrusted
        = co_await allow_untrusted_contexts(profile_world, promise.extension_id());
    world = profile_world.lock();
    if(!world) co_return;
    if(!allow_untrusted) {
        promise.reject();
        co_return;
    }
    if(!check_access(promise.security_profile(), *allow_untrusted)) {
        promise.reject(access_error());
        co_return;
    }
    if constexpr (Detail::is_read_only) {
        promise.reject("This storage area is read-only");
        co_return;
    }
    if constexpr (Detail::has_write_rate_quota) {
        if(const auto error = m_storage->consume_sync_write_quota(
            promise.extension_id())) {
            promise.reject(*error);
            co_return;
        }
    }
    orion::ExtensionStorageAreas::ClearQuery query;
    query.extension_id = promise.extension_id();
    query.storage_id = Detail::storage_id;
    auto* api = &world->core_api();
    world.reset();
    auto reply = co_await async(invoke(
        api,
        &orion::CoreApi::extension_storage_areas_clear), query);
    world = profile_world.lock();
    if(!world) co_return;
    if(reply.err) {
        promise.reject();
        co_return;
    }

    rfl::Generic::Object changes = storage_changes(reply.changes);
    if(!changes.empty()) {
        notify_changes(
            promise.extension_id(), rfl::Generic(std::move(changes)),
            *allow_untrusted);
    }
    promise.resolve();
}

template<typename Interface, typename Detail>
detached_task
StorageAreaV2<Interface, Detail>::setAccessLevel(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    v2::Promise<void> promise,
    bool allow_untrusted_contexts)
{
    auto world = profile_world.lock();
    if(!world) co_return;
    orion::ExtensionStorageAccessLevels::SetQuery query;
    query.extension_id = promise.extension_id();
    query.storage_id = Detail::storage_id;
    query.allow_untrusted_contexts = allow_untrusted_contexts;
    auto* api = &world->core_api();
    world.reset();
    const auto error = co_await async(invoke(
        api,
        &orion::CoreApi::extension_storage_access_level_set), query);
    if(profile_world.expired()) co_return;
    if(error) {
        promise.reject();
        co_return;
    }
    promise.resolve();
}

template<typename Interface, typename Detail>
void
StorageAreaV2<Interface, Detail>::notify_changes(
    const std::string& extension_id,
    const rfl::Generic& changes,
    bool allow_untrusted)
{
    if(m_profile_world.expired()) {
        return;
    }
    onChanged.notify_extension_if(
        extension_id,
        [allow_untrusted](auto&, const auto& registration) {
            return allow_untrusted
                || !is_untrusted_context(registration.security_profile);
        },
        changes);
    if(m_storage) {
        m_storage->notify_changes(
            extension_id, Detail::storage_id, changes, allow_untrusted);
    }
}

template<typename Interface, typename Detail>
async_task<std::optional<bool>>
StorageAreaV2<Interface, Detail>::allow_untrusted_contexts(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    const std::string& extension_id)
{
    auto world = profile_world.lock();
    if(!world) co_return std::nullopt;
    orion::ExtensionStorageAccessLevels::GetQuery query;
    query.extension_id = extension_id;
    query.storage_id = Detail::storage_id;
    query.default_allow_untrusted_contexts
        = Detail::allow_untrusted_contexts_by_default;
    auto* api = &world->core_api();
    world.reset();
    const auto reply = co_await async(invoke(
        api,
        &orion::CoreApi::extension_storage_access_level_get), query);
    if(profile_world.expired()) co_return std::nullopt;
    if(reply.err) {
        co_return std::nullopt;
    }
    co_return reply.allow_untrusted_contexts;
}

template<typename Interface, typename Detail>
bool
StorageAreaV2<Interface, Detail>::check_access(
    v2::SecurityProfile security_profile,
    bool allow_untrusted_contexts) const
{
    return !is_untrusted_context(security_profile)
        || allow_untrusted_contexts;
}

template<typename Interface, typename Detail>
std::string
StorageAreaV2<Interface, Detail>::access_error() const
{
    return std::string("Access to storage.") + Detail::storage_id
        + " is restricted to trusted extension contexts";
}

template class StorageAreaV2<v2::StorageAreaLocal, StorageAreaLocalV2Detail>;
template class StorageAreaV2<v2::StorageAreaSync, StorageAreaSyncV2Detail>;
template class StorageAreaV2<v2::StorageAreaManaged, StorageAreaManagedV2Detail>;
template class StorageAreaV2<v2::StorageAreaSession, StorageAreaSessionV2Detail>;

} // namespace WebExtensions
