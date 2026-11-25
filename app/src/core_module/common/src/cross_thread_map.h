#pragma once
#include <functional>
#include <mutex>

namespace orion {

template<typename K, typename T, typename DummyF = std::function<T()>, typename DallocF = std::function<void(T)>>
class CrossThreadMap {
public:
    CrossThreadMap() = delete;
    CrossThreadMap(DummyF dummy) : m_dummy_f(std::move(dummy)) {}
    CrossThreadMap(DummyF dummy, DallocF dealloc) : m_dummy_f(std::move(dummy)), m_dealloc_f(std::move(dealloc)) {}

    T find(K handle) const {
        std::lock_guard<std::mutex> lk(m_guard);
        auto it = m_data.find(handle);
        if (it != m_data.end()) {
            return it->second;
        }
        return m_dummy_f();
    }
    T find_and_erase_entry(K handle) {
        std::lock_guard<std::mutex> lk(m_guard);
        auto it = m_data.find(handle);
        if (it != m_data.end()) {
            T data = it->second;
            m_data.erase(it);
            return data;
        }
        return m_dummy_f();
    }
    void insert(K handle, T data) {
        std::lock_guard<std::mutex> lk(m_guard);
        m_data.emplace(std::move(handle), std::move(data));
    }
    void remove(K handle) {
        std::lock_guard<std::mutex> lk(m_guard);
        auto it = m_data.erase(handle);
        if (it != m_data.end()) {
            m_dealloc_f(it->second);
        }
    }
    size_t size() {
        std::lock_guard<std::mutex> lk(m_guard);
        return m_data.size();
    }

    bool empty() {
        return size() == 0;
    }

    void for_each(std::function<void(const K&, const T&)> cb) {
        std::lock_guard<std::mutex> lk(m_guard);
        for (const auto& pair : m_data) {
            cb(pair.first, pair.second);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lk(m_guard);
        for (auto& pair : m_data) {
            m_dealloc_f(pair.second);
        }
        m_data.clear();
    }

    ~CrossThreadMap() {
        clear();
    };


private:
    std::mutex m_guard;
    std::map<K, T> m_data;
    DummyF m_dummy_f = [](){ return T(); } -> T;
    DallocF m_dealloc_f = [](T data) { /*default does nothing*/ };
};
} // namespace orion
