#include <gtk/gtk.h>

#include "debounce_util.h"


namespace orion::gtk {


static gboolean dispatch_func(GSource* source, GSourceFunc callback, gpointer user_data)
{
    //NOTE: technically breaks anything that would call g_source_set_callback on our source,
    //since we never actually invoke the callback variable provided from that,
    //but something doing that is unsound code anyway
    g_source_set_ready_time(source, -1);

    
    auto* func = static_cast<std::function<void()>*>(user_data);
    if(func && *func)
    {
        (*func)();
    }

    return G_SOURCE_CONTINUE;
}

static gboolean check_func(GSource* source)
{
    gint64 ready_time = g_source_get_ready_time(source);
    if(ready_time < 0) return FALSE;
    return g_get_monotonic_time() > ready_time;
}

static void delete_cb(gpointer user_data)
{
    auto* func = static_cast<std::function<void()>*>(user_data);
    delete func;
}

#pragma region Debouncer

GSourceFuncs Debouncer::s_source_funcs = {
    .prepare = NULL,
    .check = check_func,
    .dispatch = dispatch_func,
    .finalize = NULL,
    .closure_callback = NULL,
    .closure_marshal = NULL
};

Debouncer::Debouncer() {
    m_source = g_source_new(&s_source_funcs, sizeof(GSource)); //this expects the pointer to source_funcs to stay valid throughout the existence of this GSource
    g_source_set_ready_time(m_source, -1);
    g_source_attach(m_source, NULL);
}

Debouncer::Debouncer(std::function<void()> cb, int64_t duration_us) : Debouncer() {
    set_duration(duration_us);
    set_cb(cb);
}

Debouncer::~Debouncer() {
    g_source_destroy(m_source);
    g_source_unref(m_source);
}

void Debouncer::set_cb(std::function<void()> cb) {
    m_heap_cb = new std::function<void()>(std::move(cb));
    g_source_set_callback(m_source, NULL, m_heap_cb, delete_cb);
}

void Debouncer::set_duration(int64_t duration_us) {
    m_duration_us = duration_us;
}

void Debouncer::reset_timer() {
    gint64 cb_time = g_get_monotonic_time() + m_duration_us;
    g_source_set_ready_time(m_source, cb_time);
}

void Debouncer::finish_now() {
    kill_timer();

    if(m_heap_cb && *m_heap_cb)
    {
        (*m_heap_cb)();
    }
}

void Debouncer::kill_timer() {
    g_source_set_ready_time(m_source, -1);
}

#pragma endregion Debouncer

#pragma region Batcher


GSourceFuncs Batcher::s_source_funcs = {
    .prepare = NULL,
    .check = NULL,
    .dispatch = dispatch_func,
    .finalize = NULL,
    .closure_callback = NULL,
    .closure_marshal = NULL
};

Batcher::Batcher() {
    m_source = g_source_new(&s_source_funcs, sizeof(GSource)); //this expects the pointer to source_funcs to stay valid throughout the existence of this GSource
    g_source_set_ready_time(m_source, -1);
    g_source_attach(m_source, NULL);
}

Batcher::Batcher(std::function<void()> cb, int64_t duration_us) : Batcher() {
    set_duration(duration_us);
    set_cb(cb);
}

Batcher::~Batcher() {
    g_source_destroy(m_source);
    g_source_unref(m_source);
}

void Batcher::set_cb(std::function<void()> cb) {
    m_heap_cb = new std::function<void()>(cb);
    g_source_set_callback(m_source, NULL, m_heap_cb, delete_cb);
}

void Batcher::set_duration(int64_t duration_us) {
    m_duration_us = duration_us;
}

void Batcher::run() {
    if(g_source_get_ready_time(m_source) < 0)
    {
        gint64 cb_time = g_get_monotonic_time() + m_duration_us;
        g_source_set_ready_time(m_source, cb_time);
    }
}

void Batcher::finish_now() {
    if(m_heap_cb && *m_heap_cb)
    {
        (*m_heap_cb)();
    }
    kill_timer();
}

void Batcher::kill_timer() {
    g_source_set_ready_time(m_source, -1);
}

#pragma endregion Batcher

}