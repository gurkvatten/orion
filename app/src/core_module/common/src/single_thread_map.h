#pragma once
#include <functional>
#include <mutex>

namespace orion {

template<typename K, typename T, typename DummyF = std::function<T()>, typename DallocF = std::function<void(T)>>
class SingleThreadMap {
public:
    SingleThreadMap() = delete;
    SingleThreadMap(DummyF dummy) : m_dummy_f(std::move(dummy)) {}
    SingleThreadMap(DummyF dummy, DallocF dealloc) : m_dummy_f(std::move(dummy)), m_dealloc_f(std::move(dealloc)) {}

    T find(K handle) const {
        auto it = m_data.find(handle);
        if (it != m_data.end()) {
            return it->second;
        }
        return m_dummy_f();
    }
    T find_and_erase_entry(K handle) {
        auto it = m_data.find(handle);
        if (it != m_data.end()) {
            T data = it->second;
            m_data.erase(it);
            return data;
        }
        return m_dummy_f();
    }
    void insert(K handle, T data) {
        m_data.emplace(std::move(handle), std::move(data));
    }
    void remove(K handle) {
        auto it = m_data.erase(handle);
        if (it != m_data.end()) {
            m_dealloc_f(it->second);
        }
    }
    size_t size() {
        return m_data.size();
    }

    bool empty() {
        return size() == 0;
    }

    void for_each(std::function<void(const K&, const T&)> cb) {
        for (const auto& pair : m_data) {
            cb(pair.first, pair.second);
        }
    }

    void clear() {
        for (auto& pair : m_data) {
            m_dealloc_f(pair.second);
        }
        m_data.clear();
    }

    ~SingleThreadMap() {
        clear();
    };


private:
    std::map<K, T> m_data;
    DummyF m_dummy_f = [](){ return T(); } -> T;
    DallocF m_dealloc_f = [](T data) { /*default does nothing*/ };
};
} // namespace orion
