#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#pragma once
#include <string>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <memory>
#include <functional>
#include "../main_window/main_window.h"

class PermissionHandler {

public:
    enum class TypeOfDialog{
        binary,
        quadruplet
    };

    virtual void allow_persistently(const std::string& website) = 0;
    virtual void deny_persistently(const std::string& website) = 0;
    virtual std::string primary_description(std::string website) = 0;
    virtual std::string secondary_description() = 0;
    virtual TypeOfDialog get_type_of_dialog() = 0;
    virtual bool is_auto_deny_on_http() = 0;
    virtual void start_request(
        const std::string& website,
        std::shared_ptr<orion::gtk::GObjectPtr> request,
        std::function<void(bool)> callback
    ) = 0;

    // Returns nullptr if the request type is not supported
    static PermissionHandler* get_static_permission_handler_for(
        WebKitPermissionRequest* request,
        orion::ProfileId profile_id
    );
};

#endif