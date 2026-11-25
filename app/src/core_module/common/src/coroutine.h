#pragma once

#include <coroutine>
#include <cstddef>
#include <exception>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <src/dispatch.h>
#include <common/src/function_signature.h>

/*

Note that .then() or .detach() must be called on the returned async_task<T> object, otherwise
the segfault will probably occur.

// Example macro usage:
#define AWAIT(function, ...) co_await async(invoke(global_core_api,&orion::CoreApi::function),##__VA_ARGS__)

async_task<int>
async_example() {
    [[maybe_unused]] orion::GetProfileIdsReply reply1 = co_await async([](orion::Cb<orion::GetProfileIdsReply> cb){
        global_core_api->get_profile_ids(cb);
    });
    [[maybe_unused]] orion::GetProfileIdsReply reply2 = co_await async(invoke(global_core_api, &orion::CoreApi::get_profile_ids));
    [[maybe_unused]] orion::GetProfileIdsReply reply3 = AWAIT(get_profile_ids);
    co_return 42;
}

void
sync_example() {
    async_example().then([](int value){
        printf("%d\n", value);
    });
}

Unfortunately, global_core_api->get_setting<T> template method cannot be used with AWAIT()
at the moment, an attempt was made:
    typedef orion::settings::Setting<decltype(orion::settings::tabs::tab_layout)> SettingType;
    using SettingType = orion::settings::Setting<decltype(orion::settings::tabs::tab_layout)>;
    orion::GetSettingReply<orion::settings::TabLayout> tab_layout = AWAIT(get_setting<SettingType>, profile_id, orion::settings::tabs::tab_layout);

*/

template <typename... Ts>
struct last_type;

template <typename T>
struct last_type<T> {
    using type = T;
};

template <typename T, typename... Ts>
struct last_type<T, Ts...> {
    using type = typename last_type<Ts...>::type;
};

template <typename T>
struct function_traits;

// Specialization for member function pointer (const)
template <typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) const> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
};

// Specialization for member function pointer (non-const)
template <typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
};

// Primary fallback for generic functors / lambdas
template <typename T>
struct function_traits : function_traits<decltype(&T::operator())> {};

// Plain function type
template <typename R, typename... Args>
struct function_traits<R(Args...)> {
    using return_type = R;
    using args_tuple  = std::tuple<Args...>;
};

// Function pointer
template <typename R, typename... Args>
struct function_traits<R(*)(Args...)> {
    using return_type = R;
    using args_tuple  = std::tuple<Args...>;
};

// Function reference
template <typename R, typename... Args>
struct function_traits<R(&)(Args...)> : function_traits<R(*)(Args...)> {};

// std::function
template <typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> {
    using return_type = R;
    using args_tuple  = std::tuple<Args...>;
};

template <typename F>
using last_arg_t = typename last_type<
    std::tuple_element_t<
        std::tuple_size<typename function_traits<F>::args_tuple>::value - 1,
        typename function_traits<F>::args_tuple
    >
>::type;

template <typename F>
inline constexpr size_t function_arity_v =
    std::tuple_size_v<typename function_traits<F>::args_tuple>;

template <typename Callback, size_t Arity = function_arity_v<Callback>>
struct async_callback_reply;

template <typename Callback>
struct async_callback_reply<Callback, 0> {
    using type = void;
};

template <typename Callback>
struct async_callback_reply<Callback, 1> {
    using type = std::remove_cvref_t<last_arg_t<Callback>>;
};

template <typename Callback>
using async_callback_reply_t = typename async_callback_reply<Callback>::type;

template <typename Func, typename... Args>
requires (function_arity_v<last_arg_t<std::decay_t<Func>>> > 0)
[[nodiscard]] auto async(Func&& func, Args... args) {
    struct Awaiter {
        using FunctionType = std::decay_t<Func>;
        using CallbackType = last_arg_t<FunctionType>;
        using ResultType = last_arg_t<CallbackType>;
        using BareResultType = std::remove_cvref_t<ResultType>;

        Func&& f;
        std::tuple<Args...> tup;
        BareResultType result{};
        std::coroutine_handle<> handle {};

        bool await_ready() const noexcept { return false; }

        void
        await_suspend(std::coroutine_handle<> h)
        {
            handle = h;
            std::apply(
                [&](auto&&... unpacked) {
                    f(std::forward<decltype(unpacked)>(unpacked)...,
                        [this](ResultType r) {
                            result = r;
                            handle.resume();
                        }
                    );
                },
                tup
            );
        }

        BareResultType await_resume() { return result; }
    };

    return Awaiter{std::forward<Func>(func),
                   std::make_tuple(std::forward<Args>(args)...)};
}

template <typename Func, typename... Args>
requires (function_arity_v<last_arg_t<std::decay_t<Func>>> == 0)
[[nodiscard]] auto async(Func&& func, Args... args) {
    struct Awaiter {
        Func&& f;
        std::tuple<Args...> tup;
        std::coroutine_handle<> handle {};

        bool await_ready() const noexcept { return false; }

        void
        await_suspend(std::coroutine_handle<> h)
        {
            handle = h;
            std::apply(
                [&](auto&&... unpacked) {
                    f(std::forward<decltype(unpacked)>(unpacked)...,
                        [this]() { handle.resume(); });
                },
                tup);
        }

        void await_resume() const noexcept {}
    };

    return Awaiter{std::forward<Func>(func),
                   std::make_tuple(std::forward<Args>(args)...)};
}

template <typename T>
struct [[nodiscard]] async_task {
    struct promise_type {
        T value{};
        std::exception_ptr error;
        std::coroutine_handle<> continuation;
        std::function<void(T)> callback;

        async_task
        get_return_object()
        {
            return async_task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        auto
        final_suspend() noexcept
        {
            struct awaiter {
                bool await_ready() noexcept { return false; }
                void await_resume() noexcept {}
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    auto &p = h.promise();
                    if (p.continuation) {
                        // normal co_await continuation
                        p.continuation.resume();
                    } else if (p.callback) {
                        // detached with callback
                        p.callback(std::move(p.value));
                    }
                }
            };
            return awaiter{};
        }

        void unhandled_exception() { error = std::current_exception(); }
        void return_value(T v) { value = v; }
    };

    using handle_type = std::coroutine_handle<promise_type>;
    handle_type coro;

    explicit async_task(handle_type h) : coro(h) {}
    ~async_task() { if (coro) coro.destroy(); }

    // 👇 awaiter
    struct awaiter {
        handle_type coro;
        bool await_ready() const noexcept { return false; }

        void
        await_suspend(std::coroutine_handle<> h) noexcept
        {
            coro.promise().continuation = h; // remember who to wake up
            coro.resume();                   // start child coroutine
        }

        T
        await_resume()
        {
            if (coro.promise().error)
                std::rethrow_exception(coro.promise().error);
            return std::move(coro.promise().value);
        }
    };

    auto operator co_await() { return awaiter{coro}; }

    void start() { coro.resume(); }

    void resume() { coro.resume(); }

    void detach() { coro.resume(); coro = nullptr; }

    void detach(std::function<void(T)> cb) { coro.promise().callback = std::move(cb); coro.resume(); coro = nullptr; }

    void then(std::function<void(T)> cb) { coro.promise().callback = std::move(cb); coro.resume(); coro = nullptr; }

};

template <>
struct [[nodiscard]] async_task<void> {
    struct promise_type {
        std::exception_ptr error;
        std::coroutine_handle<> continuation;
        std::function<void()> callback;

        async_task
        get_return_object()
        {
            return async_task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        auto
        final_suspend() noexcept
        {
            struct awaiter {
                bool await_ready() noexcept { return false; }
                void await_resume() noexcept {}
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    auto &p = h.promise();
                    if (p.continuation) {
                        p.continuation.resume();
                    } else if (p.callback) {
                        p.callback();
                    }
                }
            };
            return awaiter{};
        }

        void unhandled_exception() { error = std::current_exception(); }
        void return_void() noexcept {}
    };

    using handle_type = std::coroutine_handle<promise_type>;
    handle_type coro;

    explicit async_task(handle_type h) : coro(h) {}
    ~async_task() { if (coro) coro.destroy(); }

    struct awaiter {
        handle_type coro;
        bool await_ready() const noexcept { return false; }

        void
        await_suspend(std::coroutine_handle<> h) noexcept
        {
            coro.promise().continuation = h;
            coro.resume();
        }

        void
        await_resume()
        {
            if (coro.promise().error)
                std::rethrow_exception(coro.promise().error);
        }
    };

    auto operator co_await() { return awaiter{coro}; }

    void start() { coro.resume(); }

    void resume() { coro.resume(); }

    void detach() { coro.resume(); coro = nullptr; }

    void detach(std::function<void()> cb) { coro.promise().callback = std::move(cb); coro.resume(); coro = nullptr; }

    void then(std::function<void()> cb) { coro.promise().callback = std::move(cb); coro.resume(); coro = nullptr; }
};

// The terminal static_assert lets invocation sites use a conventional trailing
// semicolon without leaving an extra semicolon after the generated definition.
#define ASYNC_TASK_REPLY_WRAPPER(ClassName, MethodName) \
    using MethodName##_callback_member = decltype(&ClassName::MethodName); \
    using MethodName##_callback_arg = last_arg_t<MethodName##_callback_member>; \
    using MethodName##_reply_arg = async_callback_reply_t<MethodName##_callback_arg>; \
    template <typename... RequestArgs> \
    requires std::is_invocable_v< \
        MethodName##_callback_member, ClassName&, RequestArgs..., MethodName##_callback_arg> \
    async_task<MethodName##_reply_arg> async_##MethodName(RequestArgs... request_args) { \
        constexpr auto callback_member = static_cast<MethodName##_callback_member>( \
            &ClassName::MethodName); \
        auto call = [this, callback_member](RequestArgs... args, MethodName##_callback_arg reply) { \
            (this->*callback_member)(std::move(args)..., std::move(reply)); \
        }; \
        if constexpr (std::is_void_v<MethodName##_reply_arg>) { \
            co_await async(call, std::move(request_args)...); \
            co_return; \
        } else { \
            co_return co_await async(call, std::move(request_args)...); \
        } \
    } \
    static_assert(true)

struct detached_task {
    struct promise_type {
        detached_task get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }

        auto
        final_suspend() noexcept
        {
            struct {
                bool await_ready() noexcept { return false; }
                void await_resume() noexcept {}
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept { h.destroy(); }
            } awaiter;
            return awaiter;
        }

        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};

template <typename Obj, typename R, typename... Args>
auto
invoke(std::shared_ptr<Obj> obj, R(Obj::*memfn)(Args...))
{
    return std::function<R(Args...)>(
        [obj, memfn](Args... args) {
            return (obj.get()->*memfn)(args...);
        }
    );
}

// const member function version
template <typename Obj, typename R, typename... Args>
auto
invoke(std::shared_ptr<Obj> obj, R(Obj::*memfn)(Args...) const)
{
    return std::function<R(Args...)>(
        [obj, memfn](Args... args) {
            return (obj.get()->*memfn)(args...);
        }
    );
}

template <typename Obj, typename R, typename... Args>
auto
invoke(Obj* obj, R(Obj::*memfn)(Args...))
{
    return std::function<R(Args...)>(
        [obj, memfn](Args... args) {
            return (obj->*memfn)(args...);
        }
    );
}

// const member function version
template <typename Obj, typename R, typename... Args>
auto
invoke(Obj* obj, R(Obj::*memfn)(Args...) const)
{
    return std::function<R(Args...)>(
        [obj, memfn](Args... args) {
            return (obj->*memfn)(args...);
        }
    );
}

template<typename _Rep, typename _Period>
auto
wait_for(const std::chrono::duration<_Rep, _Period>& time)
{
    return async([](double seconds, std::function<void(orion::Error)> cb) -> void {
        dispatch(orion::on_current_thread(), orion::after(std::chrono::milliseconds((int64_t)(1000 * seconds))), [cb]() {
            cb(orion::Error::ok());
        });
    }, std::chrono::duration<double>(time).count());
}

namespace orion {

template <typename T>
class AsyncInitObject { //for singlethreaded use only

private:
    enum class State { Empty, Initializing, Ready };

    T m_value;
    State m_state;
    std::vector<std::coroutine_handle<>> m_waiters;

public:
    struct InitAwaiter {
        friend AsyncInitObject;
    
    protected:
        InitAwaiter(AsyncInitObject& self) : m_self(self) {}
        AsyncInitObject& m_self;
    
    public:
        bool chosen_to_initialise = false;

        bool await_ready() {
            return m_self.m_state == State::Ready;
        }

        bool await_suspend(std::coroutine_handle<> handle) {

            switch(m_self.m_state)
            {
                case State::Ready:
                    return false;
                case State::Empty:
                    m_self.m_state = State::Initializing;
                    chosen_to_initialise = true;
                    return false;
                case State::Initializing:
                    m_self.m_waiters.push_back(handle);
                    return true;
            }

            orion::log_error("%s: wrong State enum value", FUNCTION_SIGNATURE);
            return false;
        }

        [[nodiscard]] bool await_resume() {
            return chosen_to_initialise;
        }

    };

    InitAwaiter wait_or_init() {
        return InitAwaiter{*this};
    }

    void set_value_and_resume(T new_value) {
        m_value = std::move(new_value);
        m_state = State::Ready;
        for(auto h : m_waiters)
        {
            h.resume();
        }
    }

    T get() {
        g_return_val_if_fail(m_state == State::Ready, "");
        return m_value;
    }
};


}


using namespace std::chrono_literals;
