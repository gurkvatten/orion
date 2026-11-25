#pragma once
#include <gtk/gtk.h>

#include <functional>

namespace orion::gtk {

 //NOTE: this class assumes that it is only ever touched from the glib main thread
class Debouncer {
    
public:
    Debouncer();
    Debouncer(std::function<void()> cb, int64_t duration_us);
    ~Debouncer();

    void set_cb(std::function<void()> cb);
    void set_duration(int64_t duration_us); //does not apply retroactivelly

    Debouncer(const Debouncer&) = delete;
    Debouncer operator=(const Debouncer&) = delete;
    Debouncer(Debouncer&&) = delete;
    Debouncer operator=(Debouncer&&) = delete;

    void reset_timer();
    void finish_now();
    void kill_timer();
private:
    GSource* m_source = nullptr;
    int64_t m_duration_us;
    std::function<void()>* m_heap_cb = nullptr; //we aren't the actual owner here, the GSource object is

    static GSourceFuncs s_source_funcs;
};

 //NOTE: this class assumes that it is only ever touched from the glib main thread
class Batcher {
public:
    Batcher();
    Batcher(std::function<void()> cb, int64_t duration_us);
    ~Batcher();

    void set_cb(std::function<void()> cb);
    void set_duration(int64_t duration_us); //does not apply retroactivelly

    Batcher(const Batcher&) = delete;
    Batcher operator=(const Batcher&) = delete;
    Batcher(Batcher&&) = delete;
    Batcher operator=(Batcher&&) = delete;


    void run();
    void finish_now();
    void kill_timer();

private:
    GSource* m_source = nullptr;
    int64_t m_duration_us;
    std::function<void()>* m_heap_cb = nullptr; //we aren't the actual owner here, the GSource object is

    static GSourceFuncs s_source_funcs;
};

}