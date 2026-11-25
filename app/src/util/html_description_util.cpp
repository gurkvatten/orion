#include "html_description_util.h"
#include "util/jsc_util.h"

namespace orion::gtk {

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

//js script to look for either a description meta or the first paragraph of the document and extract text
#define description_scrape_js \
"function getPageDescription() {"\
  "const meta = document.querySelector('meta[name=\"description\"]');"\
  "if (meta && meta.content) {"\
    "return meta.content;"\
  "}"\
  "const p = document.querySelector('p');"\
  "if (p) {"\
    "return p.textContent.trim();"\
  "}"\
  "return null;"\
"}"\
"getPageDescription();"

template<typename T>
struct TArgs {
    void(*callback)(const char*, T*);
    T* data;
};

static void
webview_evaluate_js_callback(GObject* object, GAsyncResult* result, gpointer user_data)
{
    TArgs<void>* args = static_cast<TArgs<void>*>(user_data);
    JSCValue* val;
    GError* err = NULL;
    val = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(object), result, &err);
    if(!err)
    {
        if(jsc_value_is_string(val))
        {
            std::string str = orion::jsc::value_to_string(val);
            JSCException* exception = jsc_context_get_exception(jsc_value_get_context(val));
            if(exception)
            {
                orion::log_error("Error scraping html description: %s", jsc_exception_get_message(exception));
                args->callback(NULL, args->data);
            }
            else
            {
                args->callback(str.c_str(), args->data);
            }
        }
        else
        {
            args->callback(NULL, args->data);
        }
    }
    else
    {
        orion::log_error("Error scraping html description: %s", err->message);
        g_error_free(err);
        args->callback(NULL, args->data);
    }
    if(val) g_object_unref(val);
    g_object_unref(object);
    g_free(args);
}

static void
webview_load_changed_callback(WebKitWebView* self, WebKitLoadEvent load_event, gpointer user_data)
{
    if(load_event == WEBKIT_LOAD_FINISHED) 
    {
        webkit_web_view_evaluate_javascript(self, description_scrape_js, -1, NULL, NULL, NULL, webview_evaluate_js_callback, user_data);
        g_signal_handlers_disconnect_by_data(self, user_data);
    }
}

static void
webview_load_failed_callback(WebKitWebView* self, WebKitLoadEvent load_event, gchar* failing_uri,
    GError* error, gpointer user_data)
{
    //intend to just fail silently, passing a null pointer to the callback
    TArgs<void>* args = static_cast<TArgs<void>*>(user_data);
    args->callback(NULL, args->data);
    g_object_unref(self);
    g_signal_handlers_disconnect_by_data(self, user_data);
}

struct NetworkSessionWrapper {
    NetworkSessionWrapper() = delete;
    NetworkSessionWrapper(const NetworkSessionWrapper&) = delete;
    NetworkSessionWrapper(WebKitNetworkSession* session) {
        this->session = session;
    }

    ~NetworkSessionWrapper() {
        g_clear_object(&(this->session));
    }

    WebKitNetworkSession* get_session() {
        return this->session;
    }

private:
    WebKitNetworkSession* session;
};


WebKitNetworkSession*
get_ephemeral_network_session()
{
    static NetworkSessionWrapper wrapper(webkit_network_session_new_ephemeral());
    // Enable ITP (Intelligent Tracking Prevention) by default
    webkit_network_session_set_itp_enabled(wrapper.get_session(), TRUE);
    return wrapper.get_session();
}

#endif

template<typename T> void
execute_with_uri_description(void(*cb)(const char*, T*), const gchar* uri, T* data)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    TArgs<T>* webkit_callback_args = g_new(TArgs<T>, 1);
    *webkit_callback_args = {.callback = cb, .data = data};
    if(strlen(uri))
    {
        WebKitWebView* view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, "network-session", get_ephemeral_network_session(), NULL));
        webkit_web_view_load_uri(view, uri);
        g_object_ref_sink(view);
        g_signal_connect(view, "load-changed", G_CALLBACK(webview_load_changed_callback), webkit_callback_args);
        g_signal_connect(view, "load-failed", G_CALLBACK(webview_load_failed_callback), webkit_callback_args);
    }
    else
    {
        cb(NULL, data);
    }

#else

    cb("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nullam turpis turpis, imperdiet eu imperdiet eget, ultricies nec quam. Duis eu ipsum non dui feugiat pellentesque.", data);
#endif
}

template void execute_with_uri_description<orion::ReadingListEntryId>(
    void(*cb)(const char*, orion::ReadingListEntryId*), const gchar* uri, orion::ReadingListEntryId* data
);

void
load_gtk_label_from_uri_description(GtkLabel* label, const gchar* uri)
{
    g_object_ref(label);
    execute_with_uri_description<GtkLabel>([](const char* desc, GtkLabel* data){
        if(desc) gtk_label_set_label(data, desc);
        g_object_unref(data);
    }, uri, label);
}

} // namespace orion::gtk