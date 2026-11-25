#include <variant>

#include <gtk/gtk.h>

#include <common/src/coroutine.h>
#include <common/src/memory.h>

namespace orion::gtk {

struct GErrorDeleter {
    void operator()(GError* err) const {
        if (err) g_error_free(err);
    }
};
using UniqueError = std::unique_ptr<GError, GErrorDeleter>;

template <typename T>
class GlibResult { //doesn't handle cleanup for T
    std::variant<T, UniqueError> m_data;

public:
    GlibResult(T val) : m_data(val) {};
    GlibResult(UniqueError err) : m_data(std::move(err)) {};

    explicit operator bool() const { return std::holds_alternative<T>(m_data); };

    T operator->() const { return std::get<T>(m_data); };
    T operator*() const { return std::get<T>(m_data); };
    T value() const { return std::get<T>(m_data); };

    const UniqueError& error() const { return std::get<UniqueError>(m_data); };
};


template <>
class GlibResult<void> {
    UniqueError m_err;

public:
    GlibResult() {};
    GlibResult(UniqueError err) : m_err(std::move(err)) {};

    explicit operator bool() const { return (bool)m_err; };

    const UniqueError& error() const { return m_err; };
};


template <typename T>
bool
log_result_error(const GlibResult<T>& res)
{
    if(!res)
    {
        orion::log_error("%s", res.error()->message);
        return true;
    }
    else
    {
        return false;
    }
}

template <auto StartFn, auto FinishFn, typename... Args>
struct GlibAwaiter {

private:
    std::tuple<Args...> m_args;
    GAsyncResult* m_res = nullptr;
    std::coroutine_handle<> m_coro;

public:
    explicit GlibAwaiter(Args... args) : m_args(std::forward<Args>(args)...) {};

    bool await_ready() {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) {

        m_coro = handle;

        GAsyncReadyCallback callback = [](GObject* source, GAsyncResult* res, gpointer data)
        {
            auto* self = static_cast<GlibAwaiter*>(data);
            self->m_res = res;
            self->m_coro.resume();
        };

        std::apply([&](auto&&... unpacked_args) {
            StartFn(unpacked_args..., callback, this);
        }, m_args);
    }

    [[nodiscard]] auto await_resume() {
        GError* err = NULL;
        auto source_obj = std::get<0>(m_args);

        //there may be some glib async funcs that only take GAsyncResult* and GError**, yell if so
        static_assert(requires { FinishFn(std::get<0>(m_args), m_res, &err); },
            "FinishFn doesn't conform to expected signature");

        using RetType = decltype(FinishFn(source_obj, m_res, &err));

        if constexpr (std::is_void_v<decltype(FinishFn(source_obj, m_res, &err))>) {
            FinishFn(source_obj, m_res, &err);
            if(err) return GlibResult<void>{UniqueError(err)};
            else return GlibResult<void>{};
        } else {
            auto result = FinishFn(source_obj, m_res, &err);
            if(err) return GlibResult<RetType>{UniqueError(err)};
            else return GlibResult<RetType>{result};
        }
    }
};

//pattern showcase: file_util.cpp: get_host_path_coro()
template <auto StartFn, auto FinishFn, typename... Args>
auto await(Args&&... args) {
    return GlibAwaiter<StartFn, FinishFn, std::decay_t<Args>...>(std::forward<Args>(args)...);
}


}