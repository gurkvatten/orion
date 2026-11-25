#include <gtk/gtk.h>

#include <functional>

#include <core/settings.h>
#include <core/downloader.h>

#include <common/src/defer.h>
#include <common/src/time_util.h>

#include "downloads_util.h"

#include "app.h"

#define FILE_SAME_NAME_LIMIT 99

namespace {
    struct FindValidTargetData {
        std::string filename;
        std::filesystem::path path;
        int index;
        orion::DownloadDestinationCallback cb;
    };
}


namespace orion {


static void find_valid_target(FindValidTargetData* data);

static void
file_creation_callback(GObject* file, GAsyncResult* result, gpointer user_data)
{
    FindValidTargetData* data = static_cast<FindValidTargetData*>(user_data);
    
    GError* err = NULL;
    //on 
    orion::gtk::GObjectPtr stream(G_OBJECT(g_file_create_finish(G_FILE(file), result, &err)));

    if(err)
    {
        defer([err]{ g_error_free(err); });
        data->index ++;
        find_valid_target(data);
    }
    else
    {
        data->cb(true, g_file_get_path(G_FILE(file)));
        delete data;
    }
}

//inserts before first dot, ignoring dots in position 0, those should not denote extension
static std::string
insert_suffix_into_filename(std::string filename, std::string suffix)
{
    size_t first_dot_pos = filename.find('.', 1);
    if(first_dot_pos == std::string::npos)
    {
        return filename + suffix;
    }
    else
    {
        return filename.substr(0, first_dot_pos) + suffix + filename.substr(first_dot_pos);
    }
}

static void
find_valid_target(FindValidTargetData* data)
{
    if(data->index == FILE_SAME_NAME_LIMIT)
    {
        orion::log_error("too many files with the same name \"%s\" in path \"%s\"", data->filename.c_str(), data->path.c_str());
        data->cb(false, "");
        delete data;
    }
    else
    {
        std::string suffix = (data->index?std::format("({})", data->index):"");
        GFile* file = g_file_new_for_path(
            (data->path/insert_suffix_into_filename(data->filename, suffix)).string().c_str());
        g_file_create_async(file, G_FILE_CREATE_NONE, G_PRIORITY_DEFAULT, NULL, file_creation_callback, data);
    }
}

    
static void
on_download_path_file_dialog_finished(GObject* dialog, GAsyncResult* result, gpointer user_data)
{
    std::unique_ptr<FindValidTargetData> data(static_cast<FindValidTargetData*>(user_data));

    GError* err = NULL;
    GFile* file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(dialog), result, &err);
    if(err)
    {
        if(err->code != GTK_DIALOG_ERROR_DISMISSED)
        {
            orion::log_error("on_download_path_file_dialog_finishedon_download_path_file_dialog_finished(): %s", err->message);
        }
        g_clear_error(&err);
        data->cb(false, "");
        return;
    }
    defer([file]{ g_object_unref(file); });

    char* path = g_file_get_path(file);
    defer([path]{ g_free(path); });
    if(path)
    {
        data->path = path;
        find_valid_target(data.release());
    }
}

void
get_download_destination(orion::ProfileId id, std::string recommended_filename, DownloadDestinationCallback&& cb)
{
    global_core_api->get_setting(id, settings::general::file_download_location, 
        [cb = std::move(cb), id = std::move(id), filename = std::move(recommended_filename)](GetSettingReply<settings::FileDownloadLocation> reply)
        {
            VERIFY_LOG(reply.err); 
            if(reply.value == settings::FileDownloadLocation::not_set)
            {
                reply.value = reply.default_value;
            }
            orion::log_verbose("%i", reply.value);
            switch(reply.value)
            {
                case settings::FileDownloadLocation::not_set:
                    {
                        cb(false, "");
                    }
                    break;
                case settings::FileDownloadLocation::downloads:
                    {
                        const gchar* path = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
                        if(path)
                        {
                            auto* data = new FindValidTargetData{};
                            data->cb = std::move(cb);
                            data->filename = std::move(filename);
                            //TODO: improve
                            data->path = path;
                            data->index = 0;
                            find_valid_target(data);
                        }
                        else
                        {
                            orion::log_error("failed to obtain user default download folder");
                            cb(false, "");
                        }
                    }
                    break;
                case settings::FileDownloadLocation::ask_for_each_download:
                    {
                        global_core_api->get_setting(id, settings::general::file_download_custom_path,
                        [cb = std::move(cb), filename = std::move(filename)](GetSettingReply<std::string> reply)
                        {
                            VERIFY_LOG(reply.err);
                            GFile* file = g_file_new_for_path(reply.value.c_str());
                            defer([file]{ g_object_unref(file); });

                            
                            GtkFileDialog* dialog = gtk_file_dialog_new();
                            defer([dialog]{ g_object_unref(dialog); });
                            gtk_file_dialog_set_initial_file(dialog, file);

                            auto* data = new FindValidTargetData{};
                            data->cb = std::move(cb);
                            data->filename = std::move(filename);
                            //TODO: more heuristics upon failure? validity checking for the path itself?
                            data->path = reply.value;
                            data->index = 0;

                            //TODO: propagate a parent widget here somehow... good luck to anybody doing that since this needs to be callable from web_view_factory
                            gtk_file_dialog_select_folder(dialog, NULL, NULL, on_download_path_file_dialog_finished, data);
                        });
                    }
                    break;
                case settings::FileDownloadLocation::custom_path:
                    {
                        global_core_api->get_setting(id, settings::general::file_download_custom_path,
                        [cb = std::move(cb), filename = std::move(filename)](GetSettingReply<std::string> reply)
                        {
                            VERIFY_LOG(reply.err);
                            if(!(reply.value.length()))
                            {
                                orion::log_error("attempting to download but custom path not set");
                                return;
                            }
                            auto* data = new FindValidTargetData{};
                            data->cb = std::move(cb);
                            data->filename = std::move(filename);
                            //TODO: more heuristics for failure? validity checking for the path itself?
                            data->path = reply.value;
                            data->index = 0;
                            find_valid_target(data);
                        });
                    }
                    break;
            }       
        });
}









#pragma region Download hijack

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
struct DownloadContextData {
    orion::DownloadId id = orion::DownloadId(0);
    bool has_failed = false;
    int64_t current_size;
    uint timeout_id;
};

static inline void
init_update_download_query_from_context_data(DownloadContextData& data, orion::UpdateDownloadQuery& query)
{
    query.update_current_size = true;
    query.current_size = data.current_size;
}

static int
download_timer_callback(gpointer data)
{
    auto& context_data = *static_cast<std::shared_ptr<DownloadContextData>*>(data);
    orion::UpdateDownloadQuery query{};
    query.id = context_data->id;
    init_update_download_query_from_context_data(*context_data, query);
    global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
    return G_SOURCE_CONTINUE;
}

static void
on_download_failed(WebKitDownload* download, GError* error, std::shared_ptr<DownloadContextData>* data)
{
    orion::log_verbose("failed");
    (*data)->has_failed = true;
    orion::UpdateDownloadQuery query{};
    query.id = (*data)->id;
    switch(error->code)
    {
        case WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER:
            query.update_operation_state = true;
            query.operation_state = orion::DownloadOperationState::cancelled;
            break;
        case WEBKIT_DOWNLOAD_ERROR_DESTINATION:
            orion::log_error("download error due to destination");
            if(error && error->message)
            {
                orion::log_error(error->message);
            }
            query.update_operation_state = true;
            query.operation_state = orion::DownloadOperationState::failed;
            break;
        case WEBKIT_DOWNLOAD_ERROR_NETWORK:
            query.update_operation_state = true;
            query.operation_state = orion::DownloadOperationState::failed;
            break;
        default:
            orion::log_error("download error with unknown code");
            query.update_operation_state = true;
            query.operation_state = orion::DownloadOperationState::failed;
            break;
    }
    global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
}

static void
on_download_finished(WebKitDownload* download, std::shared_ptr<DownloadContextData>* data)
{
    orion::log_verbose("finished");
    g_clear_handle_id(&(*data)->timeout_id, g_source_remove);
    orion::UpdateDownloadQuery query{};
    query.id = (*data)->id;
    init_update_download_query_from_context_data(**data, query);
    if(!(*data)->has_failed)
    {
        query.update_operation_state = true;
        query.operation_state = orion::DownloadOperationState::finished;
    }
    global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
}

static void
on_download_received_data(WebKitDownload* self, guint64 data_length, std::shared_ptr<DownloadContextData>* data)
{
    orion::log_verbose("received data");
    (*data)->current_size = webkit_download_get_received_data_length(self);
}

static void
on_download_created_destination(WebKitDownload* download, gchar* destination, orion::DownloadId* id)
{
    std::filesystem::path path = destination;
    orion::UpdateDownloadQuery query{};
    query.id = *id;
    query.update_path = true;
    query.path = path.parent_path();
    query.update_name = true;
    query.name = path.filename();
    global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
}

static gboolean
on_download_decide_destination(WebKitDownload* download, const gchar* suggested_filename, orion::ProfileId* profile_id)
{
    WebKitURIResponse* response = webkit_download_get_response(download);
    
    //TODO: store these for resuming download
    // SoupMessageHeaders* headers = webkit_uri_response_get_http_headers(response);
    // const char* etag_c_string = headers?soup_message_headers_get_one(headers, "ETag"):nullptr;
    // std::string etag = etag_c_string?etag_c_string:"";
    // const char* last_modified_c_string = headers?soup_message_headers_get_one(headers, "Last-Modified"):nullptr;
    // std::string last_modified = last_modified_c_string?last_modified_c_string:"";


    orion::gtk::WeakRefWrapper download_weak(download);
    //TODO: FIXME: validate filename somehow?
    std::string filename = suggested_filename?(strlen(suggested_filename)?suggested_filename:"unknown"):"unknown";
    const gchar* uri = webkit_uri_response_get_uri(response);

    GIcon* icon = g_content_type_get_icon(webkit_uri_response_get_mime_type(response));
    defer([icon]{ g_object_unref(icon); });
    gchar* icon_c_string = g_icon_to_string(icon);
    std::string icon_string = icon_c_string?icon_c_string:"";


    orion::CreateDownloadQuery query{};
    query.profile_id = *profile_id;
    query.total_size = webkit_uri_response_get_content_length(response);
    query.icon_string = icon_string;
    query.origin_uri = uri?uri:"";
    query.name = filename;
    query.timestamp = orion::tm::unix_time_utc64_now();
    global_core_api->create_download(query, [download_weak, profile_id = *profile_id, filename](orion::CreateDownloadReply reply){
        VERIFY_LOG(reply.err);

        auto download_strong = download_weak.lock();
        if(!download_strong) return;

        auto destination_cb = [download_weak, id = reply.id](bool success, std::filesystem::path target){

            auto download_strong = download_weak.lock();
            
            orion::UpdateDownloadQuery query{};
            query.id = id;
            if(!success)
            {
                query.update_operation_state = true;
                query.operation_state = orion::DownloadOperationState::failed;
            }
            if(download_strong)
            {
                WebKitDownload* download = WEBKIT_DOWNLOAD(download_strong.get());
                if(!success)
                {
                    webkit_download_cancel(download);
                }
                else
                {
                    //this is the actual start of the download, up until now, only header has been pulled
                    OrionApp* app = ORION_APP(g_application_get_default());
                    if(app)
                    {
                        orion::log_verbose("%s", target.c_str());
                        orion_app_add_download_with_id(app, download_strong.get(), id);
                        orion::DownloadId* id_ptr = new orion::DownloadId(id);
                        g_signal_connect_data(download_strong.get(), "created-destination", G_CALLBACK(on_download_created_destination), id_ptr,
                            [](gpointer data, GClosure* closure){ delete static_cast<orion::DownloadId*>(data); }, G_CONNECT_DEFAULT);
                        webkit_download_set_allow_overwrite(download, true);
                        webkit_download_set_destination(download, target.c_str());
                    }
                    else
                    {
                        query.update_operation_state = true;
                        query.operation_state = orion::DownloadOperationState::failed;
                    }
                }
            }
            query.update_path = true;
            query.path = target;
            global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
        };
        orion::get_download_destination(profile_id, filename, destination_cb);

        auto data = std::make_shared<DownloadContextData>(); 
        data->id = reply.id;
        data->has_failed = false;


        data->timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT, 400, download_timer_callback, new decltype(data)(data),
            [](gpointer data){ delete static_cast<std::shared_ptr<DownloadContextData>*>(data); });
        g_signal_connect_data(download_strong.get(), "failed", G_CALLBACK(on_download_failed), new decltype(data)(data),
            [](gpointer data, GClosure* closure){ delete static_cast<std::shared_ptr<DownloadContextData>*>(data); }, G_CONNECT_DEFAULT);
        g_signal_connect_data(download_strong.get(), "finished", G_CALLBACK(on_download_finished), new decltype(data)(data),
            [](gpointer data, GClosure* closure){ delete static_cast<std::shared_ptr<DownloadContextData>*>(data); }, G_CONNECT_DEFAULT);
        g_signal_connect_data(download_strong.get(), "received-data", G_CALLBACK(on_download_received_data), new decltype(data)(data),
            [](gpointer data, GClosure* closure){ delete static_cast<std::shared_ptr<DownloadContextData>*>(data); }, G_CONNECT_DEFAULT);

        g_object_weak_ref(download_strong.get(), [](gpointer user_data, GObject* destroyed_object){
            auto* data = static_cast<std::shared_ptr<DownloadContextData>*>(user_data);

            g_clear_handle_id(&(*data)->timeout_id, g_source_remove);

            OrionApp* app = ORION_APP(g_application_get_default());
            if(app)
            {
                orion_app_remove_download_for_id(app, (*data)->id);
            }

            delete data;
        }, new decltype(data)(data));
    });

    //pauses the download until we call decide destination on it, 
    return TRUE;
}
namespace {
    struct Cookie {
        std::string domain = "";
        std::string path = "";
        std::string name = "";
        std::string value = "";
        int max_age = -1;
        bool secure = true;
        bool http_only = true;
    };

    struct Header {
        std::string name;
        std::string value;
    };

    struct ProrgressTimerCallbackData {
        orion::DownloadId id;
        size_t total_received;
        size_t content_length;
    };
}

static inline void
prepare_update_download_query_with_progress(ProrgressTimerCallbackData& data, orion::UpdateDownloadQuery& query)
{
    query.id = data.id;
    query.update_total_size = true;
    query.total_size = std::max(data.content_length, data.total_received);
    query.update_current_size = true;
    query.current_size = data.total_received;
    // query.update_operation_state = true;
    // query.operation_state = orion::DownloadOperationState::downloading;
}

static int
download_progress_timer_callback(gpointer user_data)
{
    auto* data = static_cast<std::shared_ptr<ProrgressTimerCallbackData>*>(user_data);
    orion::UpdateDownloadQuery query{};
    prepare_update_download_query_with_progress(**data, query);
    global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
    return G_SOURCE_CONTINUE;
}

static void
on_backend_download_created_for_intercepted_download(orion::DownloadId download_id, orion::ProfileId profile_id, std::string uri_string,
    const std::vector<Header>& headers, const std::vector<Cookie>& cookies)
{
    std::shared_ptr<orion::Downloader> downloader = global_core_api->create_downloader();
    std::shared_ptr<ProrgressTimerCallbackData> progress_data = std::make_shared<ProrgressTimerCallbackData>();
    progress_data->id = download_id;

    guint timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT, 400, download_progress_timer_callback, new decltype(progress_data)(progress_data),
        [](gpointer data){ delete static_cast<std::shared_ptr<ProrgressTimerCallbackData>*>(data); });

    for(auto& header: headers)
    {
        downloader->add_header(header.name, header.value);
    }
    for(auto& cookie: cookies)
    {
        downloader->add_cookie(cookie.domain, cookie.path, cookie.name, cookie.value, cookie.max_age, cookie.secure, cookie.http_only);
    }
    
    downloader->set_uri(uri_string);
    auto progress_cb = [id = download_id, progress_data](const orion::Downloader::Progress& progress){
        progress_data->content_length = progress.content_length;
        progress_data->total_received = progress.total_received;
    };
    downloader->set_progress_cb(progress_cb);



    auto finished_cb = [id = download_id, timeout_id, progress_data](orion::Error err){
        g_source_remove(timeout_id);
        auto* id_ptr = new orion::DownloadId(id);
        //deferring destruction of downloader so that it does not happen while inside its callback
        g_idle_add_full(G_PRIORITY_DEFAULT, [](gpointer user_data){
            
            auto* id_ptr = static_cast<orion::DownloadId*>(user_data);
            OrionApp* app = ORION_APP(g_application_get_default());
            orion_app_remove_download_for_id(app, *id_ptr);
            return G_SOURCE_REMOVE;

        }, id_ptr, [](gpointer data){

            delete static_cast<orion::DownloadId*>(data);
        });

        orion::UpdateDownloadQuery query{};
        query.id = id;
        if(err)
        {
            bool is_cancelled = err.code() == orion::ErrorCode::cancelled;
            query.operation_state = is_cancelled
                ? orion::DownloadOperationState::cancelled
                : orion::DownloadOperationState::failed;
            query.update_operation_state = true;
        }
        else
        {
            query.operation_state = orion::DownloadOperationState::finished;
            query.update_operation_state = true;
        }
        prepare_update_download_query_with_progress(*progress_data, query);
        global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
        if(err)
        {
            if(err.code() == orion::ErrorCode::cancelled)
                orion::log_info("%s", err.to_string().c_str());
            else
                orion::log_error("%s", err.to_string().c_str());
        }
    };
    downloader->set_finished_cb(finished_cb);



    auto recommended_filename_cb = [download_id, profile_id, downloader](const std::string& filename){
        orion::get_download_destination(profile_id, filename, [download_id, downloader](bool success, std::filesystem::path path){
            if(!success)
            {
                downloader->cancel();
                return;
            }
            downloader->set_overwrite_existing(true);
            downloader->set_target_path(path);

            orion::UpdateDownloadQuery query{};

            query.id = download_id;
            query.update_name = true;
            query.name = path.filename();
            query.update_path = true;
            query.path = path.parent_path();
            global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
        });
    };
    downloader->set_recommended_filename_cb(recommended_filename_cb);

    auto content_type_cb = [download_id](const std::string& content_type){
        GIcon* icon = g_content_type_get_icon(content_type.c_str());
        defer([icon]{ g_object_unref(icon); });
        gchar* icon_c_string = g_icon_to_string(icon);

        orion::UpdateDownloadQuery query{};

        query.id = download_id;
        query.update_icon_string = true;
        query.icon_string = icon_c_string?icon_c_string:"";
        global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
    };
    downloader->set_content_type_cb(content_type_cb);



    downloader->start_download();
    OrionApp* app = ORION_APP(g_application_get_default());
    orion_app_add_download_with_id(app, downloader, download_id);
}

namespace {
    struct CookieManagerGetCookiesUserData {
        std::string uri_string{};
        orion::ProfileId profile_id{};
        std::vector<Header> headers{};
        std::vector<Cookie> cookies{};
    };
}

static void
on_cookie_manager_get_cookies_finish(GObject* source, GAsyncResult* result, gpointer user_data)
{
    auto* data = static_cast<std::shared_ptr<CookieManagerGetCookiesUserData>*>(user_data);

    GError* err = NULL;
    //TODO: actually use these cookies for our download
    GList* cookie_list = webkit_cookie_manager_get_cookies_finish(WEBKIT_COOKIE_MANAGER(source), result, &err);
    if(err)
    {
        defer([err]{ g_error_free(err); });
        orion::log_error("%s", err->message);
        return;
    }
    auto cookie_free = [](gpointer cookie){ soup_cookie_free(static_cast<SoupCookie*>(cookie)); };
    g_list_free_full(cookie_list, cookie_free);

    orion::CreateDownloadQuery query{};
    query.timestamp = orion::tm::unix_time_utc64_now();
    query.operation_state = orion::DownloadOperationState::downloading;
    query.origin_uri = data->get()->uri_string;
    query.profile_id = data->get()->profile_id;
    global_core_api->create_download(query, [data = *data](orion::CreateDownloadReply reply) {
        VERIFY_LOG(reply.err);
        on_backend_download_created_for_intercepted_download(reply.id, data->profile_id, data->uri_string, data->headers, data->cookies);
    });
}

void
on_download_started(WebKitNetworkSession* self, WebKitDownload* download, orion::ProfileId* profile_id)
{
    //NOTE: everything that webkit attempts to download to a file is passed through here
    //Since webkitgtk lacks an api to pause downloads and erases all temp files on cancel,
    //we need to itercept what we can and pass it off to a custom downloader.
    //This sadly likely double requests, but I cannot think of an alternative
    //Options I've thought of:
    //implement pause api on webkitgtk's side
    //  - highly do not wish to maintain a fork of webkit
    //intercept all navigation requests, handle all downloads ourselves, feed to webview using load_bytes
    //  - cannot feed webkit the http header data in that case
    //
    //All in all, this double request behaviour might happen with things like Internet Download Manager anyway,
    //meaning this might be a rough industry standard
    orion::log_verbose("on_download_started");
    WebKitURIRequest* request = webkit_download_get_request(download);
    std::string uri_string = webkit_uri_request_get_uri(request);
    orion::Uri uri = orion::Uri::parse_strict(uri_string.c_str());
    if(uri.known_scheme() == orion::Uri::Scheme::http || uri.known_scheme() == orion::Uri::Scheme::https)
    {
        std::string_view method_string = webkit_uri_request_get_http_method(request);
        if(method_string == SOUP_METHOD_GET)
        {
            auto* data_ptr_heap = new std::shared_ptr<CookieManagerGetCookiesUserData>(std::make_shared<CookieManagerGetCookiesUserData>());

            CookieManagerGetCookiesUserData* user_data = data_ptr_heap->get();
            user_data->profile_id = *profile_id;
            user_data->uri_string = uri_string;

            SoupMessageHeaders* headers = webkit_uri_request_get_http_headers   (request);
            soup_message_headers_foreach(headers, [](const char* name, const char* value, gpointer user_data){
                auto* headers_vec = static_cast<std::vector<Header>*>(user_data);
                //TODO: filter;
                headers_vec->push_back({name, value});
            }, &user_data->headers);
            webkit_download_cancel(download);

            WebKitCookieManager* cookie_manager = webkit_network_session_get_cookie_manager(self);
            webkit_cookie_manager_get_cookies(cookie_manager, uri_string.c_str(), NULL, on_cookie_manager_get_cookies_finish, data_ptr_heap);

            return;
        }
    }


    //creating a new profile id copy is a safety precaution to avoid issues when network session dies before the download object -- unlikely
    orion::ProfileId* profile_id_copy = new orion::ProfileId(*profile_id);
    g_signal_connect_data(download, "decide-destination", G_CALLBACK(on_download_decide_destination), profile_id_copy,
        [](gpointer data, GClosure* closure){ delete static_cast<orion::ProfileId*>(data); }, G_CONNECT_DEFAULT);
}

#endif

#pragma endregion Download hijack




} // namespace orion