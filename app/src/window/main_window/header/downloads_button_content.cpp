#include "downloads_button_content.h"

#include <common/src/defer.h>

#include "window/main_window/main_window.h"
#include "window/orion_gizmo.h"

namespace {
    struct CppWrapper {
        orion::ObserverId downloads_observer;
        std::map<orion::DownloadId, std::pair<int64_t, int64_t>> active_downloads;
    };
}

struct _DownloadsButtonContent
{
    GtkWidget parent_type;

    GtkExpressionWatch* profile_id_watch;
    GtkImage* image;
    OrionGizmo* progress_cone;
    OrionGizmo* finished_marker;
    GtkWidget* failed_marker;
    float progress;

    CppWrapper cpp_data;
};

G_DEFINE_TYPE(DownloadsButtonContent, downloads_button_content, GTK_TYPE_WIDGET)

static void
downloads_button_content_finalize(GObject* gobject)
{
    DownloadsButtonContent* self = APP_DOWNLOADS_BUTTON_CONTENT(gobject);
    std::destroy_at(&self->cpp_data);
    G_OBJECT_CLASS(downloads_button_content_parent_class)->finalize(gobject);
}

static void
downloads_button_content_dispose(GObject* gobject)
{
    DownloadsButtonContent* self = APP_DOWNLOADS_BUTTON_CONTENT(gobject);
    if(self->cpp_data.downloads_observer != orion::ObserverId(0))
    {
        global_core_api->downloads_change().remove_observer(self->cpp_data.downloads_observer);
        self->cpp_data.downloads_observer = orion::ObserverId(0);
    }
    if(self->profile_id_watch)
    {
        gtk_expression_watch_unwatch(self->profile_id_watch);
        self->profile_id_watch = NULL;
    }
    if(self->image)
    {
        gtk_widget_unparent(GTK_WIDGET(self->image));
        self->image = NULL;
    }
    if(self->progress_cone)
    {
        gtk_widget_unparent(GTK_WIDGET(self->progress_cone));
        self->progress_cone = NULL;
    }
    if(self->finished_marker)
    {
        gtk_widget_unparent(GTK_WIDGET(self->finished_marker));
        self->finished_marker = NULL;
    }
    if(self->failed_marker)
    {
        gtk_widget_unparent(GTK_WIDGET(self->failed_marker));
        self->finished_marker = NULL;
    }
    G_OBJECT_CLASS(downloads_button_content_parent_class)->dispose(gobject);
}

static void
downloads_button_content_set_show_failed_marker(DownloadsButtonContent* self, bool show_failed_marker)
{
    gtk_widget_set_visible(self->failed_marker, show_failed_marker);
}

static void
downloads_button_content_set_show_finished_marker(DownloadsButtonContent* self, bool show_finished_marker)
{
    gtk_widget_set_visible(GTK_WIDGET(self->finished_marker), show_finished_marker);
}

void
downloads_button_content_notify_click(DownloadsButtonContent* self)
{
    downloads_button_content_set_show_failed_marker(self, false);
    downloads_button_content_set_show_finished_marker(self, false);
}

static void
downloads_button_content_active_downloads_notify(DownloadsButtonContent* self)
{
    //TODO:
    int64_t total_size = 0;
    int64_t current_size = 0;
    for(const auto& data: self->cpp_data.active_downloads)
    {
        if(data.second.second > 0)
        {
            current_size += data.second.first;
            total_size += data.second.second;
        }
    }
    if(total_size)
    {
        self->progress = ((float)current_size)/((float)total_size);
    }
    else
    {
        self->progress = 0.0f;
    }
    gtk_widget_queue_draw(GTK_WIDGET(self->progress_cone));
}

static void
downloads_button_content_bind_to_profile_id(DownloadsButtonContent* self, orion::ProfileId profile_id)
{
    orion::gtk::WeakRefWrapper self_weak(self);

    orion::GetDownloadIdsQuery query{};
    query.profile_id = profile_id;
    global_core_api->get_download_ids(query, [self_weak, profile_id](orion::GetDownloadIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_downloads(reply.ids, [self_weak, profile_id](orion::GetDownloadsReply reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if(self_strong)
            {
                DownloadsButtonContent* self = APP_DOWNLOADS_BUTTON_CONTENT(self_strong.get());

                if(self->cpp_data.downloads_observer != orion::ObserverId(0))
                {
                    global_core_api->downloads_change().remove_observer(self->cpp_data.downloads_observer);
                    self->cpp_data.downloads_observer = orion::ObserverId(0);
                }
                self->cpp_data.active_downloads.clear();

                for(auto& download_item : reply.downloads)
                {
                    if(download_item.operation_state == orion::DownloadOperationState::downloading)
                    {
                        self->cpp_data.active_downloads.insert_or_assign(download_item.id, std::pair{download_item.current_size, download_item.total_size});
                    }
                }
                downloads_button_content_active_downloads_notify(self);

                self->cpp_data.downloads_observer = global_core_api->downloads_change().add_observer(
                [self_weak, profile_id](const orion::DownloadsChange& change) {
                    auto self_strong = self_weak.lock();
                    if(self_strong)
                    {
                        DownloadsButtonContent* self = APP_DOWNLOADS_BUTTON_CONTENT(self_strong.get());
                        bool should_notify = false;
                        for(auto& created : change.created)
                        {
                            if(created.profile_id == profile_id)
                            {
                                if(created.operation_state == orion::DownloadOperationState::downloading)
                                {
                                    self->cpp_data.active_downloads.insert_or_assign(created.id, std::pair{created.current_size, created.total_size});
                                    should_notify = true;
                                }
                            }
                        }
                        for(auto& updated : change.updated)
                        {
                            if(updated.profile_id == profile_id)
                            {
                                if(updated.operation_state == orion::DownloadOperationState::downloading)
                                {
                                    self->cpp_data.active_downloads.insert_or_assign(updated.id, std::pair{updated.current_size, updated.total_size});
                                    should_notify = true;
                                }
                                else if(self->cpp_data.active_downloads.erase(updated.id))
                                {
                                    should_notify = true;
                                    if(updated.operation_state == orion::DownloadOperationState::finished)
                                    {
                                        downloads_button_content_set_show_finished_marker(self, true);
                                    }
                                    else if(updated.operation_state == orion::DownloadOperationState::failed)
                                    {
                                        downloads_button_content_set_show_failed_marker(self, true);
                                    }
                                }
                            }
                        }
                        for(auto& deleted : change.deleted)
                        {
                            if(self->cpp_data.active_downloads.erase(deleted))
                            {
                                should_notify = true;
                            }
                        }
                        if(should_notify)
                        {
                            downloads_button_content_active_downloads_notify(self);
                        }
                    }
                });
            }
        });
    });
}

static void
on_root_profile_id_changed(gpointer user_data)
{
    DownloadsButtonContent* button = APP_DOWNLOADS_BUTTON_CONTENT(user_data);
    GValue val = G_VALUE_INIT;
    if(gtk_expression_watch_evaluate(button->profile_id_watch, &val))
    {
        downloads_button_content_bind_to_profile_id(button, orion::ProfileId(g_value_get_int64(&val)));
    }
    g_value_unset(&val);
}

static void
downloads_button_content_progress_cone_snapshot(GtkWidget* widget, GtkSnapshot* snapshot, DownloadsButtonContent* content)
{
    if(content->progress <= 0.0f)
    {
        return;
    }

    float progress = content->progress;
    
    const float line_width = 2;
    int width = gtk_widget_get_width (widget);
    int height = gtk_widget_get_height (widget);
    float center_x = width / 2.0f;
    float center_y = height / 2.0f;
    float radius = MIN(center_x, center_y)-(line_width/2);

    GdkRGBA color;
    gtk_widget_get_color(widget, &color);
    if(progress >= 1.0)
    {
        //for now ignore
    }
    else
    {
        GskPathBuilder* builder = gsk_path_builder_new();
        float start_angle = -G_PI_2;
        float end_angle = start_angle + (progress * 2.0f * (float)G_PI);
        
        float end_x = center_x + radius*std::cos(end_angle);
        float end_y = center_y + radius*std::sin(end_angle);

        gsk_path_builder_move_to(builder, center_x, center_y - radius);
        gboolean is_convex_angle = progress<=0.5;

        gsk_path_builder_svg_arc_to(builder, radius, radius, 0.0f, !is_convex_angle, true, end_x, end_y);

        GskPath_autoptr path = gsk_path_builder_free_to_path(builder);
        GskStroke_autoptr stroke = gsk_stroke_new(line_width);
        gsk_stroke_set_line_cap(stroke, GSK_LINE_CAP_ROUND);
        gtk_snapshot_append_stroke(snapshot, path, stroke, &color);
    }
    //graphene_rect_t bounds{{.x = 0, .y = 0}, {.width = (float)width, .height = (float)height}};
    //gtk_snapshot_append_color(snapshot, &color, &bounds);
}

static void
downloads_button_content_init(DownloadsButtonContent* self)
{
    std::construct_at(&self->cpp_data);

    GtkWidget* image = gtk_image_new();
    self->image = GTK_IMAGE(image);
    gtk_image_set_from_icon_name(self->image, "arrow-pointing-at-line-down");
    gtk_widget_set_parent(image, GTK_WIDGET(self));

    self->progress_cone = orion_gizmo_new(nullptr, nullptr, [content = self](GtkWidget* widget, GtkSnapshot* snapshot){
        downloads_button_content_progress_cone_snapshot(widget, snapshot, content);
    });
    gtk_widget_add_css_class(GTK_WIDGET(self->progress_cone), "accent");
    gtk_widget_set_parent(GTK_WIDGET(self->progress_cone), GTK_WIDGET(self));

    self->finished_marker = orion_gizmo_new(nullptr, nullptr, nullptr);
    gtk_widget_add_css_class(GTK_WIDGET(self->finished_marker), "finished-marker");
    gtk_widget_set_visible(GTK_WIDGET(self->finished_marker), false);
    gtk_widget_set_parent(GTK_WIDGET(self->finished_marker), GTK_WIDGET(self));

    self->failed_marker = gtk_inscription_new("!");
    gtk_widget_add_css_class(self->failed_marker, "error");
    gtk_widget_add_css_class(self->failed_marker, "failed-marker");
    gtk_widget_set_visible(self->failed_marker, false);
    gtk_widget_set_parent(self->failed_marker, GTK_WIDGET(self));


    GtkExpression* root_expression = gtk_property_expression_new(GTK_TYPE_WIDGET, NULL, "root");
    GtkExpression* profile_id_expression = gtk_property_expression_new(MAIN_WINDOW_TYPE, root_expression, "profile-id");
    defer([profile_id_expression]{ gtk_expression_unref(profile_id_expression); });
    self->profile_id_watch = gtk_expression_watch(profile_id_expression, self, on_root_profile_id_changed, self, NULL);
}

static GtkSizeRequestMode
downloads_button_content_get_request_mode(GtkWidget* widget)
{
    DownloadsButtonContent* self = APP_DOWNLOADS_BUTTON_CONTENT(widget);
    return gtk_widget_get_request_mode(GTK_WIDGET(self->image));
}

static void
downloads_button_content_measure(GtkWidget* widget, GtkOrientation orientation, int for_size, int* min, int* nat, int* min_baseline, int* nat_baseline)
{
    DownloadsButtonContent* self = APP_DOWNLOADS_BUTTON_CONTENT(widget);
    gtk_widget_measure(GTK_WIDGET(self->image), orientation, for_size, min, nat, min_baseline, nat_baseline);
}

static void
downloads_button_content_size_allocate(GtkWidget* widget, int width, int height, int baseline)
{
    DownloadsButtonContent* self = APP_DOWNLOADS_BUTTON_CONTENT(widget);
    gtk_widget_allocate(GTK_WIDGET(self->image), width, height, baseline, NULL);
    gtk_widget_allocate(GTK_WIDGET(self->progress_cone), width, height, baseline, NULL);
    {
        GtkWidget* finished_marker = GTK_WIDGET(self->finished_marker);

        int child_width, child_height;
        gtk_widget_measure(finished_marker, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &child_width, NULL, NULL);
        gtk_widget_measure(finished_marker, GTK_ORIENTATION_VERTICAL, -1, NULL, &child_height, NULL, NULL);

        graphene_point_t child_origin = {.x = (float)width-child_width, .y = 0.0f};
        GskTransform* finished_marker_transform = gsk_transform_translate(NULL, &child_origin);
        gtk_widget_allocate(finished_marker, child_width, child_height, 0, finished_marker_transform);
    }
    {
        GtkWidget* failed_marker = self->failed_marker;

        int child_width, child_height;
        gtk_widget_measure(failed_marker, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &child_width, NULL, NULL);
        gtk_widget_measure(failed_marker, GTK_ORIENTATION_VERTICAL, -1, NULL, &child_height, NULL, NULL);

        graphene_point_t child_origin = {.x = 0.0, .y = 0.0f};
        GskTransform* failed_marker_transform = gsk_transform_translate(NULL, &child_origin);
        gtk_widget_allocate(failed_marker, child_width, child_height, 0, failed_marker_transform);
    }
}

static void
downloads_button_content_snapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    for(GtkWidget* child = gtk_widget_get_first_child(widget); child; child = gtk_widget_get_next_sibling(child))
    {
        gtk_widget_snapshot_child(widget, child, snapshot);
    }
}



static void
downloads_button_content_class_init(DownloadsButtonContentClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    obj_class->dispose = downloads_button_content_dispose;
    obj_class->finalize = downloads_button_content_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->get_request_mode = downloads_button_content_get_request_mode;
    widget_class->measure = downloads_button_content_measure;
    widget_class->size_allocate = downloads_button_content_size_allocate;
    widget_class->snapshot = downloads_button_content_snapshot;
}
