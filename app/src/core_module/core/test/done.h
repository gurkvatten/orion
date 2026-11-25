#pragma once

#include <functional>
#include <common/src/error.h>
#include <core/core_api.h>

#define DONE_CHECK(err, done) \
    do { \
        orion::Error done_check_err_macro_variable__; \
        done_check_err_macro_variable__ = err; \
        if (done_check_err_macro_variable__) { \
            VERIFY_LOG(done->fail(WRAP_ERROR(done_check_err_macro_variable__))); \
            return; \
        } \
    } while (0)

// We need to avoid multiple evaluation of the condition
#define DONE_ASSERT_2(done, condition) \
    do { \
        if (!(condition)) { \
            orion::Error done_assert_err_macro_variable__; \
            done_assert_err_macro_variable__ = MAKE_ERROR(orion::ErrorCode::assert, "check failed: %s", #condition); \
            VERIFY_LOG(done->fail(done_assert_err_macro_variable__)); \
            return; \
        } \
    } while (0)

// We need to avoid multiple evaluation of the condition
#define DONE_ASSERT_3(done, condition, message) \
    do { \
        if (!(condition)) { \
            orion::Error done_assert_err_macro_variable__; \
            done_assert_err_macro_variable__ = MAKE_ERROR(orion::ErrorCode::assert, "check failed: %s (message: %s)", #condition, message); \
            VERIFY_LOG(done->fail(done_assert_err_macro_variable__)); \
            return; \
        } \
    } while (0)

#define GET_DONE_ASSERT_MACRO(_1, _2, _3, NAME, ...) NAME
#define DONE_ASSERT(...) GET_DONE_ASSERT_MACRO(__VA_ARGS__, DONE_ASSERT_3, DONE_ASSERT_2)(__VA_ARGS__)

namespace orion {

class Done {
public:
    Done(CoreApi* core_api, std::function<void(orion::Error)> cb)
        : m_expect_count(1)
        , m_cb(std::move(cb))
        , m_core_api(core_api)
        {}

    Done(int expect_count, CoreApi* core_api, std::function<void(orion::Error)> cb)
        : m_expect_count(expect_count)
        , m_cb(std::move(cb))
        , m_core_api(core_api)
        {}

    orion::Error
    ok()
    {
        if (m_count >= m_expect_count) {
            RAISE(orion::ErrorCode::assert, "ok() called more times than expected");
        }
        m_count++;
        try_invoke_cb();
        SUCCEED();
    }

    orion::Error
    fail(const orion::Error& err)
    {
        if (m_count >= m_expect_count) {
            RAISE(orion::ErrorCode::assert, "fail() called more times than expected");
        }
        if (!err) {
            RAISE(orion::ErrorCode::assert, "fail() called with success error");
        }

        m_count++;
        m_errors.push_back(err);
        try_invoke_cb();
        SUCCEED();
    }

    void
    set_expect_count(int count)
    {
        m_expect_count = count;
    }

private:
    void
    try_invoke_cb()
    {
        if (m_count == m_expect_count) {
            auto&& cb = std::move(m_cb);
            if (m_errors.empty()) {
                if (m_core_api) {
                    // We need dispatch this asynchronously to avoid missing observer callback notifications
                    m_core_api->async([cb]{ cb(orion::Error::ok()); });
                }
            } else {
                auto&& err = m_errors.front();
                if (m_core_api) {
                    // We need dispatch this asynchronously to avoid missing observer callback notifications
                    m_core_api->async([cb,err]{ cb(err); }); // call with the first error
                }
            }
        }
    }

private:
    int m_expect_count;
    int m_count = 0;
    std::function<void(orion::Error)> m_cb;
    std::vector<orion::Error> m_errors;
    CoreApi* m_core_api{};
};

} // namespace orion
