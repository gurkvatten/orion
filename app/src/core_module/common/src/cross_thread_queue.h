#pragma once
#include <functional>
#include <condition_variable>
#include <mutex>

namespace orion {
template<typename T>
class CrossThreadQueue {
public:
    using Cb = std::function<T(T data)>;
public:
    void push(T t) {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_queue.emplace_back(std::move(t));
        }
        m_cv.notify_one();
    }
    void clear_and_push(T t) {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_queue.clear();
            m_queue.emplace_back(std::move(t));
        }
        m_cv.notify_one();
    }
    T wait_and_pop() {
        std::unique_lock<std::mutex> lk(m_mutex);
        if (!m_queue.empty()) {
            return _pop();
        }
        m_cv.wait(lk, [this]()->bool{ return !m_queue.empty(); });
        if (m_queue.empty()) {
            throw std::runtime_error("Failed to wait for queue element");
        }
        T t = std::move(m_queue.front());
        m_queue.erase(m_queue.begin());
        return t;
    }
    bool empty() {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_queue.empty();
    }
    T peek() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_queue.empty()) {
            return T{};
        }
        return m_queue.front();
    }
    T wait_for_a_peek(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lk(m_mutex);
        if (!m_queue.empty()) {
            return m_queue.front();
        }
        m_cv.wait_for(lk, timeout, [this]()->bool{ return !m_queue.empty(); });
        if (m_queue.empty()) {
            return T(); // timeout
        }
        return m_queue.front();
    }
    T wait_for_and_pop(std::chrono::milliseconds timeout, Cb cb = nullptr) {
        std::unique_lock<std::mutex> lk(m_mutex);
        if (!m_queue.empty()) {
            return _pop();
        }
        m_cv.wait_for(lk, timeout, [this]()->bool{ return !m_queue.empty(); });
        if (m_queue.empty()) {
            return T(); // timeout
        }
        return cb ? cb(_pop()) : _pop();
    }
    T pop(Cb cb = nullptr) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_queue.empty()) {
            return T{};
        }
        return cb ? cb(_pop()) : _pop();
    }

private:
    T _pop() {
        T t = std::move(m_queue.front());
        m_queue.erase(m_queue.begin());
        return t;
    }

private:
    std::vector<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};
} // namespace orion