#define ENABLE_DISPATCH_TEST 0
#define ENABLE_VERBOSE_LOGS 1

#include <src/dispatch_test.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <set>

using namespace std::chrono_literals;

namespace orion {

class Test {
public:
    std::mutex mtx;
    std::condition_variable cv;
    std::thread thread;
    std::string name;
    std::set<std::string> expect_list;
    std::multiset<std::string> done_list;
    bool ordered = false;
    std::vector<std::string> ordered_parts;
    std::vector<std::string>::iterator ordered_it;

    void
    done(std::string part)
    {
        std::lock_guard<std::mutex> lock(mtx);
        log_verbose((name + "::" + part).c_str());

        if (expect_list.count(part) == 0) {
            log_error("dispatch test failed (name: %s): unexpected call: %s", name.c_str(), part.c_str());
            assert(false);
        }

        if (done_list.count(part) > 0) {
            log_error("dispatch test failed (name: %s): duplicate call: %s", name.c_str(), part.c_str());
            assert(false);
        }

        if (ordered) {
            if (ordered_it == ordered_parts.end()) {
                log_error("dispatch test failed (name: %s): unexpected call: %s", name.c_str(), part.c_str());
                assert(false);
            }
            if (*ordered_it != part) {
                log_error("dispatch test failed (name: %s): out of order call: %s (expected: %s)", name.c_str(), part.c_str(), ordered_it->c_str());
                assert(false);
            }
            ordered_it++;
        }

        done_list.insert(part);
        cv.notify_one();
    }
};

std::shared_ptr<Test> g_test_do_job;

void
job_done()
{
    assert(is_main_thread());
    g_test_do_job->done("then");
}

void
job_fail(Error err)
{
    assert(false);
}

std::string
join(const std::vector<std::string>& parts, const std::string& sep)
{
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];
        if (i + 1 < parts.size()) result += sep;
    }
    return result;
}

std::shared_ptr<Test>
make_test(std::string name, std::chrono::milliseconds timeout, std::initializer_list<std::string> expects)
{
    auto test = std::make_shared<Test>();
    test->name = name;
    test->expect_list = expects;
    test->ordered_parts = std::vector<std::string>(expects);
    test->ordered_it = test->ordered_parts.begin();
    test->thread = std::thread([test, timeout, name]{
        std::unique_lock<std::mutex> lock(test->mtx);
        auto result = test->cv.wait_for(lock, timeout, [test]{
            for (auto part : test->expect_list) {
                if (test->done_list.count(part) == 0) {
                    return false;
                }
            }
            return true;
        });
        if (!result) {
            std::vector<std::string> missing;
            for (auto part : test->expect_list) {
                if (test->done_list.count(part) == 0) {
                    missing.push_back(part);
                }
            }
            log_error("dispatch test failed (name: %s): missing invocations: [%s]", name.c_str(), join(missing, " ").c_str());
            assert(false);
        }
    });
    test->thread.detach();
    log_verbose("running test: %s", name.c_str());
    return test;
}

void
dispatch_test_internal()
{
    {
        auto test = make_test("dispatch(on_main, ordered)", 1s, {"work[0]", "work[1]", "work[2]", "work[3]", "then[0]", "then[1]", "then[2]", "then[3]"});
        test->ordered = true;
        dispatch(on_main_thread(), [test]{
            assert(is_main_thread());
            test->done("work[0]");
            SUCCEED();
        }).then([test]{
            assert(is_main_thread());
            test->done("then[0]");
        });
        dispatch(on_main_thread(), [test]{
            assert(is_main_thread());
            test->done("work[1]");
            SUCCEED();
        }).then([test]{
            assert(is_main_thread());
            test->done("then[1]");
        });
        dispatch(on_main_thread(), [test]{
            assert(is_main_thread());
            test->done("work[2]");
            SUCCEED();
        }).then([test]{
            assert(is_main_thread());
            test->done("then[2]");
        });
        dispatch(on_main_thread(), [test]{
            assert(is_main_thread());
            test->done("work[3]");
            SUCCEED();
        }).then([test]{
            assert(is_main_thread());
            test->done("then[3]");
        });
    }
    {
        // Then calls in this test must be called in order, but they still may be interleaved with work calls
        // because both groups are dispatched on different threads. But this is not very probable.
        auto test = make_test("dispatch(on_disk, ordered)", 1s, {"work[0]", "work[1]", "work[2]", "work[3]", "then[0]", "then[1]", "then[2]", "then[3]"});
        test->ordered = true;
        dispatch(on_disk_thread(), [test]{
            assert(is_disk_thread());
            test->done("work[0]");
            SUCCEED();
        }).then([test]{
            assert(is_main_thread());
            test->done("then[0]");
        });
        dispatch(on_disk_thread(), [test]{
            assert(is_disk_thread());
            test->done("work[1]");
            SUCCEED();
        }).then([test]{
            assert(is_main_thread());
            test->done("then[1]");
        });
        dispatch(on_disk_thread(), [test]{
            assert(is_disk_thread());
            test->done("work[2]");
            SUCCEED();
        }).then([test]{
            assert(is_main_thread());
            test->done("then[2]");
        });
        dispatch(on_disk_thread(), [test]{
            assert(is_disk_thread());
            test->done("work[3]");
            SUCCEED();
        }).then([test]{
            assert(is_main_thread());
            test->done("then[3]");
        });
    }
    {
        auto test = make_test("dispatch(on_disk, after(6ms))", 5s, {"work", "then_or_error"});
        dispatch(on_disk_thread(), after(6ms), [test]{
            test->done("work");
            RAISE(ErrorCode::invalid_state, "test error");
        }).then_or_error([test]{
            test->done("then_or_error");
        });
    }
    {
        auto test = make_test("dispatch(on_network, after(8ms))", 5s, {"work", "then_or_error"});
        dispatch(on_network_thread(), after(8ms), [test]{
            test->done("work");
            RAISE(ErrorCode::invalid_state, "test error");
        }).then_or_error([test](Error err){
            test->done("then_or_error");
            assert(err.code() == ErrorCode::invalid_state);
        });
    }

    {
        auto test = make_test("group", 5s, {"[0]::work", "[0]::then", "[1]::work", "[1]::then"});
        auto group = {
            // note: this dispatches the items in the group eagerly (immediately),
            // and the group cannot be reused at the moment
            dispatch([test]{
                test->done("[0]::work");
                SUCCEED();
            }).then([test]{
                test->done("[0]::then");
            }),
            dispatch([test]{
                test->done("[1]::work");
                SUCCEED();
            }).then([test]{
                test->done("[1]::then");
            }),
        };

        when_all(group);
        //when_any(group); // this does not work - we cannot reuse the same dispatched objects
    }

    {
        auto test = make_test("when_any()", 5s, {"dispatch(1)::work", "dispatch(2)::work", "error"});
        when_any({
            dispatch(on_cpu_thread(), after(1s), [test]{
                assert(is_cpu_thread());
                test->done("dispatch(1)::work");
                SUCCEED();
            }).then([]{}).error([](Error){}),
            dispatch(on_disk_thread(), after(500ms), [test]{
                assert(is_disk_thread());
                test->done("dispatch(2)::work");
                //SUCCEED();
                RAISE(ErrorCode::invalid_state, "test");
            }).then([]{}).error([](Error){}),
        }).then([test]{
            assert(false);
            assert(is_main_thread());
            test->done("then");
        }).error([test](Error err){
            assert(is_main_thread());
            assert(err.code() == ErrorCode::invalid_state);
            test->done("error");
        });
    }
    
    {
        auto test = make_test("when_all()", 5s, {"dispatch(1)::work", "dispatch(2)::work", "error"});
        when_all({
            dispatch(on_cpu_thread(), after(1s), [test]{
                assert(is_cpu_thread());
                test->done("dispatch(1)::work");
                SUCCEED();
            }).then([]{}).error([](Error){}),
            dispatch(on_disk_thread(), after(500ms), [test]{
                assert(is_disk_thread());
                test->done("dispatch(2)::work");
                //SUCCEED();
                RAISE(ErrorCode::invalid_state, "test");
            }).then([]{}).error([](Error){}),
        }).then([]{
            assert(false);
            assert(is_main_thread());
        }).error([test](Error err){
            assert(is_main_thread());
            assert(err.code() == ErrorCode::invalid_state);
            test->done("error");
        });
    }

    {
        auto test = make_test("dispatch(on_network, after(7ms))", 5s, {"work", "error"});
        dispatch(on_network_thread(), after(7ms), [test]{
            assert(is_network_thread());
            test->done("work");
            RAISE(ErrorCode::invalid_state, "test error");
        }).error([test](Error){
            assert(is_main_thread());
            test->done("error");
        });
    }

    {
        auto test = make_test("dispatch(on_disk, after(3ms))", 5s, {"work", "then"});
        dispatch(on_disk_thread(), after(3ms), [test]{
            assert(is_disk_thread());
            test->done("work");
        }).then([test]{
            assert(is_main_thread());
            test->done("then");
        });
    }

    dispatch([]{ SUCCEED(); })
        //.then([]{}) // should cause expected assertion error
        .then([]{})
        //.error([](Error){}) // should cause expected assertion error
        .error([](Error){});

    log_verbose("ABC test start, expected: 4s -> B -> 1s -> A -> 1s -> C");
    
    dispatch(on_cpu_thread(), []{
        {
            auto test = make_test("dispatch(on_cpu, after(5s))", 10s, {"A"});
            dispatch(on_main_thread(), after(5s), [test]{
                assert(is_main_thread());
                test->done("A");
            });
        }

        log_verbose("sleeping for 1s on cpu thread");
        std::this_thread::sleep_for(1s);

        {
            auto test = make_test("dispatch(on_cpu, after(3s))", 10s, {"B"});
            dispatch(on_main_thread(), after(3s), [test]{
                assert(is_main_thread());
                test->done("B");
            });
        }

        {
            auto test = make_test("dispatch(on_cpu, after(5s))", 10s, {"C", "then"});
            dispatch(on_main_thread(), after(5s), [test]{
                assert(is_main_thread());
                test->done("C");
            }).then([test]{
                test->done("then");
                log_debug("dispatch test done");
            });
        }
    });

    {
        auto test = make_test("dispatch(on_main, after(1s))", 5s, {"work"});
        dispatch(on_main_thread(), after(1s), [test]{
            assert(is_main_thread());
            test->done("work");
        });
    }

    {
        auto test = make_test("dispatch::weak_ptr[0]", 5s, {"work", "error"});
        auto variable = std::make_shared<std::atomic<bool>>(false);
        dispatch([weak_var = std::move(std::weak_ptr(variable)), test]{
            assert(is_main_thread());
            test->done("work");
            auto var = weak_var.lock();
            if (var) {
                assert(false); // probably should not happen (but depends on the timing)
            }
            RAISE(ErrorCode::invalid_state, "test main error");
        }).then([]{
            assert(false);
            assert(is_main_thread());
        }).error([weak_var = std::move(std::weak_ptr(variable)), test](auto err){
            assert(is_main_thread());
            assert(err.code() == ErrorCode::invalid_state);
            assert(err.message() == "test main error");
            test->done("error");
            auto var = weak_var.lock();
            if (var) {
                assert(false); // probably should not happen (but depends on the timing)
            }
        });
    }

    {
        auto test = make_test("dispatch(on_network, after(1s))", 5s, {"work"});
        dispatch(on_network_thread(), after(1s), [test]{
            assert(is_network_thread());
            test->done("work");
        });
    }

    {
        auto test = make_test("dispatch(on_disk)", 5s, {"work", "then"});
        auto variable = std::make_shared<std::atomic<bool>>(false);
        dispatch(on_disk_thread(), [weak_var = std::weak_ptr(variable), test](){
            assert(is_disk_thread());
            test->done("work");
            std::this_thread::sleep_for(500ms);
            auto var = weak_var.lock();
            if (var) {
                assert(false); // probably should not happen (but depends on the timing)
            }
            auto start = std::chrono::steady_clock::now();
            auto test2 = make_test("dispatch(on_disk)::dispatch(on_main)", 8s, {"work", "then"});
            dispatch(on_main_thread(), after(5s), [start, test2]{
                assert(is_main_thread());
                test2->done("work");
                log_verbose("dispatch(on_disk)::dispatch(on_main)::work: after:%dms",
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
                );
                SUCCEED();
            }).then([test2]{
                assert(is_disk_thread());
                test2->done("then");
            });
            SUCCEED();
        }).then([weak_var = std::weak_ptr(variable), test](){
            assert(is_main_thread());
            test->done("then");
            auto test = weak_var.lock();
            if (test) {
                assert(false); // probably should not happen (but depends on the timing)
            }
        }).error([](Error err){
            assert(is_main_thread());
            assert(false);
        });
    }

    {
        auto var = std::make_shared<std::atomic<bool>>(false);
        g_test_do_job = make_test("do_job", 5s, {"work", "then"}); // "then" and "error" are not expected here
        auto do_job = []() {
            assert(is_main_thread());
            g_test_do_job->done("work");
            SUCCEED();
        };
        
        dispatch(std::move(do_job)) // std::move is optional
            .then(std::move(job_done))
            .error(std::move(job_fail));
    }
}

void
when_all_test()
{
    {
        auto test = make_test("group", 5s, {"[0]::work", "[0]::then", "[1]::work", "[1]::then"});
        auto group = {
            // note: this dispatches the items in the group eagerly (immediately),
            // and the group cannot be reused at the moment
            dispatch([test]{
                test->done("[0]::work");
                SUCCEED();
            }).then([test]{
                test->done("[0]::then");
            }),
            dispatch([test]{
                test->done("[1]::work");
                SUCCEED();
            }).then([test]{
                test->done("[1]::then");
            }),
        };

        when_all(group);
        //when_any(group); // this does not work - we cannot reuse the same dispatched objects
    }
}

void
basic_test()
{
    auto test = make_test("basic_test", 5s, {"[0]::work", "[0]::then"});
    dispatch([test]{
        test->done("[0]::work");
        SUCCEED();
    }, "basic_test").then([test]{
        test->done("[0]::then");
    });
}

void
dispatch_weak_ptr_test()
{
    auto test = make_test("dispatch::weak_ptr[0]", 5s, {"work", "error"});
    auto variable = std::make_shared<std::atomic<bool>>(false);
    dispatch([weak_var = std::move(std::weak_ptr(variable)), test]{
        assert(is_main_thread());
        test->done("work");
        auto var = weak_var.lock();
        if (var) {
            assert(false); // probably should not happen (but depends on the timing)
        }
        RAISE(ErrorCode::invalid_state, "test main error");
    }, "dispatch::weak_ptr[0]").then([]{
        assert(false);
        assert(is_main_thread());
    }).error([weak_var = std::move(std::weak_ptr(variable)), test](auto err){
        assert(is_main_thread());
        assert(err.code() == ErrorCode::invalid_state);
        assert(err.message() == "test main error");
        test->done("error");
        auto var = weak_var.lock();
        if (var) {
            assert(false); // probably should not happen (but depends on the timing)
        }
    });
}

void
dispatch_test()
{
#if ENABLE_DISPATCH_TEST
    log_debug("running dispatch test");

    // We want the dispatch test itself to be dispatched because otherwise
    // the timings (when dispatching on different threads) would be off,
    // and we want to dispatch it with a delay to avoid clashing with
    // the main window's initialization.
    auto test = make_test("dispatch_test", 20s, {"work"});

    dispatch(on_main_thread(), after(1s), [test]{
        assert(is_main_thread());
        test->done("work");
        dispatch_test_internal();
        //dispatch_weak_ptr_test();
        //when_all_test();
        //basic_test();
        log_debug("dispatch test done");
    });
#endif
}

} // namespace orion