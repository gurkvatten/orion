#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <common/src/log.h>

#include "app.h"
#include "certificate_info_dialog.h"

struct _CertificateInfoDialog
{
    AdwDialog parent;

    GtkLabel* title_label;
    GtkListBox* certificate_list;
    GtkBox* details_box;

    gchar* uri;
    GTlsCertificateFlags tls_errors;
};

G_DEFINE_TYPE(CertificateInfoDialog, certificate_info_dialog, ADW_TYPE_DIALOG)

static void certificate_info_dialog_populate_details(CertificateInfoDialog* self, GTlsCertificate* certificate);

static void
clear_box(GtkBox* box)
{
    while (GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(box))) {
        gtk_box_remove(box, child);
    }
}

static void
clear_list_box(GtkListBox* list_box)
{
    while (GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(list_box))) {
        gtk_list_box_remove(list_box, child);
    }
}

static std::string
trim(std::string value)
{
    const size_t start = value.find_first_not_of(" \t\n\r\"");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = value.find_last_not_of(" \t\n\r\"");
    return value.substr(start, end - start + 1);
}

static std::string
certificate_subject(GTlsCertificate* certificate)
{
    if (!certificate) {
        return _("Unknown certificate");
    }

    gchar* subject = g_tls_certificate_get_subject_name(certificate);
    if (!subject) {
        return _("Unknown certificate");
    }

    std::string result = subject;
    g_free(subject);
    return result;
}

static std::string
certificate_issuer(GTlsCertificate* certificate)
{
    if (!certificate) {
        return _("Unknown issuer");
    }

    gchar* issuer = g_tls_certificate_get_issuer_name(certificate);
    if (!issuer) {
        return _("Unknown issuer");
    }

    std::string result = issuer;
    g_free(issuer);
    return result;
}

static std::string
common_name_from_distinguished_name(const std::string& name)
{
    size_t start = name.find("CN=");
    size_t prefix_len = 3;
    if (start == std::string::npos) {
        start = name.find("commonName=");
        prefix_len = 11;
    }

    if (start == std::string::npos) {
        return "";
    }

    start += prefix_len;
    size_t end = name.find(',', start);
    if (end == std::string::npos) {
        end = name.length();
    }

    return trim(name.substr(start, end - start));
}

static std::string
certificate_display_name(GTlsCertificate* certificate)
{
    std::string subject = certificate_subject(certificate);
    std::string common_name = common_name_from_distinguished_name(subject);
    return common_name.empty() ? subject : common_name;
}

static std::string
certificate_issuer_display_name(GTlsCertificate* certificate)
{
    std::string issuer = certificate_issuer(certificate);
    std::string common_name = common_name_from_distinguished_name(issuer);
    return common_name.empty() ? issuer : common_name;
}

static std::string
format_date_time(GDateTime* date_time)
{
    if (!date_time) {
        return _("Unknown");
    }

    gchar* formatted = g_date_time_format(date_time, "%Y-%m-%d %H:%M:%S %Z");
    if (!formatted) {
        return _("Unknown");
    }

    std::string result = formatted;
    g_free(formatted);
    return result;
}

static std::string
format_local_long_date_time(GDateTime* date_time)
{
    if (!date_time) {
        return _("Unknown");
    }

    GDateTime* local = g_date_time_to_local(date_time);
    if (!local) {
        return _("Unknown");
    }

    gchar* formatted = g_date_time_format(local, "%A %d %B %Y at %H:%M:%S %Z");
    std::string result = formatted ? formatted : _("Unknown");
    g_free(formatted);
    g_date_time_unref(local);
    return result;
}

static std::string
certificate_date_before(GTlsCertificate* certificate)
{
    GDateTime* date_time = g_tls_certificate_get_not_valid_before(certificate);
    std::string result = format_date_time(date_time);
    if (date_time) {
        g_date_time_unref(date_time);
    }
    return result;
}

static std::string
certificate_date_after(GTlsCertificate* certificate)
{
    GDateTime* date_time = g_tls_certificate_get_not_valid_after(certificate);
    std::string result = format_date_time(date_time);
    if (date_time) {
        g_date_time_unref(date_time);
    }
    return result;
}

static bool
certificate_is_expired(GTlsCertificate* certificate)
{
    GDateTime* date_time = g_tls_certificate_get_not_valid_after(certificate);
    if (!date_time) {
        return false;
    }

    GDateTime* now = g_date_time_new_now_local();
    const bool expired = g_date_time_compare(now, date_time) > 0;
    g_date_time_unref(now);
    g_date_time_unref(date_time);
    return expired;
}

static bool
certificate_is_not_yet_valid(GTlsCertificate* certificate)
{
    GDateTime* date_time = g_tls_certificate_get_not_valid_before(certificate);
    if (!date_time) {
        return false;
    }

    GDateTime* now = g_date_time_new_now_local();
    const bool not_yet_valid = g_date_time_compare(now, date_time) < 0;
    g_date_time_unref(now);
    g_date_time_unref(date_time);
    return not_yet_valid;
}

static GTlsCertificateFlags
display_tls_errors_for_certificate(GTlsCertificate* certificate, GTlsCertificateFlags tls_errors, bool is_leaf)
{
    if (!certificate) {
        return tls_errors;
    }

    GTlsCertificateFlags display_errors = static_cast<GTlsCertificateFlags>(0);
    if (is_leaf) {
        display_errors = tls_errors;
    } else {
        if (certificate_is_expired(certificate)) {
            display_errors = static_cast<GTlsCertificateFlags>(display_errors | G_TLS_CERTIFICATE_EXPIRED);
        }
        if (certificate_is_not_yet_valid(certificate)) {
            display_errors = static_cast<GTlsCertificateFlags>(display_errors | G_TLS_CERTIFICATE_NOT_ACTIVATED);
        }
    }

    return display_errors;
}

static std::string
tls_errors_text(GTlsCertificateFlags errors)
{
    if (errors == 0) {
        return _("No TLS certificate errors");
    }

    std::vector<std::string> parts;
    if (errors & G_TLS_CERTIFICATE_UNKNOWN_CA) {
        parts.emplace_back(_("Unknown certificate authority"));
    }
    if (errors & G_TLS_CERTIFICATE_BAD_IDENTITY) {
        parts.emplace_back(_("Certificate does not match this website"));
    }
    if (errors & G_TLS_CERTIFICATE_NOT_ACTIVATED) {
        parts.emplace_back(_("Certificate is not active yet"));
    }
    if (errors & G_TLS_CERTIFICATE_EXPIRED) {
        parts.emplace_back(_("Certificate has expired"));
    }
    if (errors & G_TLS_CERTIFICATE_REVOKED) {
        parts.emplace_back(_("Certificate has been revoked"));
    }
    if (errors & G_TLS_CERTIFICATE_INSECURE) {
        parts.emplace_back(_("Certificate uses an insecure algorithm"));
    }
    if (errors & G_TLS_CERTIFICATE_GENERIC_ERROR) {
        parts.emplace_back(_("Certificate validation failed"));
    }

    std::string result;
    for (const std::string& part : parts) {
        if (!result.empty()) {
            result += ", ";
        }
        result += part;
    }
    return result.empty() ? _("Certificate validation failed") : result;
}

static std::string
certificate_status_text(GTlsCertificateFlags errors)
{
    if (errors == 0) {
        return _("This certificate is valid.");
    }

    return std::string(_("This certificate is invalid: ")) + tls_errors_text(errors);
}

static std::string
join_strings(const std::vector<std::string>& values, const char* separator)
{
    std::string result;
    for (const std::string& value : values) {
        if (!result.empty()) {
            result += separator;
        }
        result += value;
    }
    return result;
}

static std::string
dns_names_text(GTlsCertificate* certificate)
{
    GPtrArray* names = g_tls_certificate_get_dns_names(certificate);
    if (!names || names->len == 0) {
        if (names) {
            g_ptr_array_unref(names);
        }
        return _("None");
    }

    std::vector<std::string> values;
    for (guint i = 0; i < names->len; ++i) {
        GBytes* bytes = static_cast<GBytes*>(g_ptr_array_index(names, i));
        gsize size = 0;
        const char* data = static_cast<const char*>(g_bytes_get_data(bytes, &size));
        if (data && size > 0) {
            values.emplace_back(data, size);
        }
    }
    g_ptr_array_unref(names);

    return values.empty() ? _("None") : join_strings(values, ", ");
}

static std::string
ip_addresses_text(GTlsCertificate* certificate)
{
    GPtrArray* addresses = g_tls_certificate_get_ip_addresses(certificate);
    if (!addresses || addresses->len == 0) {
        if (addresses) {
            g_ptr_array_unref(addresses);
        }
        return _("None");
    }

    std::vector<std::string> values;
    for (guint i = 0; i < addresses->len; ++i) {
        GInetAddress* address = G_INET_ADDRESS(g_ptr_array_index(addresses, i));
        gchar* text = g_inet_address_to_string(address);
        if (text) {
            values.emplace_back(text);
            g_free(text);
        }
    }
    g_ptr_array_unref(addresses);

    return values.empty() ? _("None") : join_strings(values, ", ");
}

static std::string
spaced_uppercase_hex(const char* hex)
{
    if (!hex) {
        return _("Unknown");
    }

    std::string result;
    for (size_t i = 0; hex[i]; ++i) {
        if (i > 0 && i % 2 == 0) {
            result += ' ';
        }
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(hex[i])));
    }
    return result;
}

static GByteArray*
certificate_der_data(GTlsCertificate* certificate)
{
    GByteArray* der = NULL;
    g_object_get(certificate, "certificate", &der, NULL);
    return der;
}

static std::string
certificate_fingerprint(GTlsCertificate* certificate, GChecksumType checksum_type)
{
    GByteArray* der = certificate_der_data(certificate);
    if (!der || der->len == 0) {
        if (der) {
            g_byte_array_unref(der);
        }
        return _("Unknown");
    }

    gchar* digest = g_compute_checksum_for_data(checksum_type, der->data, der->len);
    std::string result = spaced_uppercase_hex(digest);
    g_free(digest);
    g_byte_array_unref(der);
    return result;
}

static std::string
certificate_der_size(GTlsCertificate* certificate)
{
    GByteArray* der = certificate_der_data(certificate);
    if (!der) {
        return _("Unknown");
    }

    std::string result = std::to_string(der->len) + " bytes";
    g_byte_array_unref(der);
    return result;
}

static std::string
certificate_pem_text(GTlsCertificate* certificate)
{
    gchar* pem = NULL;
    g_object_get(certificate, "certificate-pem", &pem, NULL);
    if (!pem) {
        return _("Unknown");
    }

    std::string result = pem;
    g_free(pem);
    return result;
}

static GtkWidget*
create_label(const char* text, GtkAlign halign, bool wrap, bool selectable)
{
    GtkWidget* label = gtk_label_new(text);
    gtk_widget_set_halign(label, halign);
    gtk_label_set_xalign(GTK_LABEL(label), halign == GTK_ALIGN_END ? 1.0f : 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), wrap);
    gtk_label_set_selectable(GTK_LABEL(label), selectable);
    return label;
}

static void
limit_label_size(GtkLabel* label, int max_width_chars, int max_lines)
{
    gtk_label_set_max_width_chars(label, max_width_chars);
    gtk_label_set_lines(label, max_lines);
    gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_END);
}

static void
add_detail_row(GtkGrid* grid, int row, const char* name, const std::string& value)
{
    GtkWidget* name_label = create_label(name, GTK_ALIGN_END, false, false);
    gtk_widget_add_css_class(name_label, "title-6");
    gtk_widget_set_valign(name_label, GTK_ALIGN_START);

    GtkWidget* value_label = create_label(value.c_str(), GTK_ALIGN_START, true, true);
    gtk_widget_set_hexpand(value_label, TRUE);
    limit_label_size(GTK_LABEL(value_label), 80, 3);

    gtk_grid_attach(grid, name_label, 0, row, 1, 1);
    gtk_grid_attach(grid, value_label, 1, row, 1, 1);
}

static void
add_summary(CertificateInfoDialog* self, GtkBox* box, GTlsCertificate* certificate, GTlsCertificateFlags display_errors)
{
    GtkWidget* summary_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom(summary_box, 12);

    GtkWidget* icon = gtk_image_new_from_icon_name("application-certificate-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 40);
    gtk_widget_set_valign(icon, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(summary_box), icon);

    GtkWidget* text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(text_box, TRUE);
    gtk_box_append(GTK_BOX(summary_box), text_box);

    GtkWidget* title = create_label(certificate_display_name(certificate).c_str(), GTK_ALIGN_START, true, true);
    gtk_widget_add_css_class(title, "title-4");
    limit_label_size(GTK_LABEL(title), 70, 2);
    gtk_box_append(GTK_BOX(text_box), title);

    GtkWidget* issuer = create_label(
        (std::string(_("Issued by: ")) + certificate_issuer_display_name(certificate)).c_str(),
        GTK_ALIGN_START,
        true,
        true
    );
    limit_label_size(GTK_LABEL(issuer), 90, 2);
    gtk_box_append(GTK_BOX(text_box), issuer);

    GDateTime* expires_at = g_tls_certificate_get_not_valid_after(certificate);
    GtkWidget* expiry = create_label(
        (std::string(_("Expires: ")) + format_local_long_date_time(expires_at)).c_str(),
        GTK_ALIGN_START,
        true,
        true
    );
    limit_label_size(GTK_LABEL(expiry), 90, 2);
    gtk_box_append(GTK_BOX(text_box), expiry);
    if (expires_at) {
        g_date_time_unref(expires_at);
    }

    GtkWidget* status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(status_box, GTK_ALIGN_START);

    GtkWidget* status_icon = gtk_image_new_from_icon_name(
        display_errors == 0 ? "emblem-ok-symbolic" : "dialog-error-symbolic"
    );
    gtk_image_set_pixel_size(GTK_IMAGE(status_icon), 16);
    gtk_widget_set_valign(status_icon, GTK_ALIGN_START);
    gtk_widget_add_css_class(status_icon, display_errors == 0 ? "success" : "error");
    gtk_box_append(GTK_BOX(status_box), status_icon);

    GtkWidget* status = create_label(certificate_status_text(display_errors).c_str(), GTK_ALIGN_START, true, true);
    limit_label_size(GTK_LABEL(status), 90, 2);
    gtk_box_append(GTK_BOX(status_box), status);

    gtk_box_append(GTK_BOX(text_box), status_box);

    gtk_box_append(box, summary_box);
}

static GtkWidget*
create_mock_trust_dropdown(bool use_system_defaults)
{
    const char* system_options[] = {
        _("Use System Defaults"),
        _("Always Trust"),
        _("Never Trust"),
        NULL,
    };
    const char* policy_options[] = {
        _("no value specified"),
        _("Always Trust"),
        _("Never Trust"),
        NULL,
    };

    GtkWidget* dropdown = gtk_drop_down_new_from_strings(use_system_defaults ? system_options : policy_options);
    gtk_widget_set_sensitive(dropdown, FALSE);
    gtk_widget_set_halign(dropdown, GTK_ALIGN_START);
    return dropdown;
}

static void
add_mock_trust_controls(GtkBox* box)
{
    GtkWidget* expander = gtk_expander_new(NULL);
    gtk_widget_set_margin_bottom(expander, 12);

    GtkWidget* title = create_label(_("Trust"), GTK_ALIGN_START, false, false);
    gtk_widget_add_css_class(title, "title-6");
    gtk_expander_set_label_widget(GTK_EXPANDER(expander), title);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_margin_top(grid, 8);
    gtk_widget_set_margin_start(grid, 8);

    GtkWidget* use_label = create_label(_("When using this certificate:"), GTK_ALIGN_END, false, false);
    gtk_grid_attach(GTK_GRID(grid), use_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), create_mock_trust_dropdown(true), 1, 0, 1, 1);

    GtkWidget* ssl_label = create_label(_("Secure Sockets Layer (SSL)"), GTK_ALIGN_END, false, false);
    gtk_widget_add_css_class(ssl_label, "title-6");
    gtk_widget_set_margin_top(ssl_label, 16);
    gtk_grid_attach(GTK_GRID(grid), ssl_label, 0, 1, 1, 1);

    GtkWidget* ssl_dropdown = create_mock_trust_dropdown(false);
    gtk_widget_set_margin_top(ssl_dropdown, 16);
    gtk_grid_attach(GTK_GRID(grid), ssl_dropdown, 1, 1, 1, 1);

    GtkWidget* policy_label = create_label(_("X.509 Basic Policy"), GTK_ALIGN_END, false, false);
    gtk_widget_add_css_class(policy_label, "title-6");
    gtk_grid_attach(GTK_GRID(grid), policy_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), create_mock_trust_dropdown(false), 1, 2, 1, 1);

    gtk_expander_set_child(GTK_EXPANDER(expander), grid);
    gtk_box_append(box, expander);
}

static GtkWidget*
create_group_title(const char* text)
{
    GtkWidget* title = create_label(text, GTK_ALIGN_START, false, false);
    gtk_widget_add_css_class(title, "title-6");
    return title;
}

static void
add_raw_certificate_section(GtkBox* box, GTlsCertificate* certificate)
{
    GtkWidget* expander = gtk_expander_new(NULL);
    gtk_widget_set_margin_bottom(expander, 12);

    GtkWidget* title = create_label(_("Raw Certificate"), GTK_ALIGN_START, false, false);
    gtk_widget_add_css_class(title, "title-6");
    gtk_expander_set_label_widget(GTK_EXPANDER(expander), title);

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_margin_top(scrolled, 8);

    GtkWidget* text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_widget_add_css_class(text_view, "view");

    GtkTextBuffer* buffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(buffer, certificate_pem_text(certificate).c_str(), -1);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(text_view), buffer);
    g_object_unref(buffer);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text_view);
    gtk_expander_set_child(GTK_EXPANDER(expander), scrolled);
    gtk_box_append(box, expander);
}

static void
add_details_group(CertificateInfoDialog* self, GtkBox* box, GTlsCertificate* certificate, GTlsCertificateFlags display_errors)
{
    GtkWidget* expander = gtk_expander_new(NULL);
    gtk_widget_set_margin_bottom(expander, 12);

    GtkWidget* title = create_group_title(_("Details"));
    gtk_expander_set_label_widget(GTK_EXPANDER(expander), title);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_widget_set_hexpand(grid, TRUE);

    int row = 0;
    if (self->uri && *self->uri) {
        add_detail_row(GTK_GRID(grid), row++, _("Page"), self->uri);
    }
    add_detail_row(GTK_GRID(grid), row++, _("Subject"), certificate_subject(certificate));
    add_detail_row(GTK_GRID(grid), row++, _("Issuer"), certificate_issuer(certificate));
    add_detail_row(GTK_GRID(grid), row++, _("Not Valid Before"), certificate_date_before(certificate));
    add_detail_row(GTK_GRID(grid), row++, _("Not Valid After"), certificate_date_after(certificate));
    add_detail_row(GTK_GRID(grid), row++, _("TLS Verification"), tls_errors_text(display_errors));

    add_detail_row(GTK_GRID(grid), row++, _("DNS Names"), dns_names_text(certificate));
    add_detail_row(GTK_GRID(grid), row++, _("IP Addresses"), ip_addresses_text(certificate));
    add_detail_row(GTK_GRID(grid), row++, _("SHA-256 Fingerprint"), certificate_fingerprint(certificate, G_CHECKSUM_SHA256));
    add_detail_row(GTK_GRID(grid), row++, _("SHA-1 Fingerprint"), certificate_fingerprint(certificate, G_CHECKSUM_SHA1));
    add_detail_row(GTK_GRID(grid), row++, _("DER Size"), certificate_der_size(certificate));

    gtk_expander_set_child(GTK_EXPANDER(expander), grid);
    gtk_box_append(box, expander);
}

static void
certificate_info_dialog_populate_details(
    CertificateInfoDialog* self,
    GTlsCertificate* certificate,
    GTlsCertificateFlags display_errors)
{
    clear_box(self->details_box);

    if (!certificate) {
        GtkWidget* title = create_label(_("No certificate information available"), GTK_ALIGN_START, true, false);
        gtk_widget_add_css_class(title, "title-4");
        gtk_box_append(self->details_box, title);

        GtkWidget* body = create_label(
            _("The current page is not using HTTPS, or WebKit has not received TLS information for it yet."),
            GTK_ALIGN_START,
            true,
            false
        );
        gtk_widget_set_margin_top(body, 8);
        gtk_box_append(self->details_box, body);
        return;
    }

    add_summary(self, self->details_box, certificate, display_errors);
    add_mock_trust_controls(self->details_box);
    add_details_group(self, self->details_box, certificate, display_errors);
    add_raw_certificate_section(self->details_box, certificate);
}

static GtkWidget*
create_certificate_row(GTlsCertificate* certificate, int depth, const char* role, bool is_leaf)
{
    GtkWidget* row = gtk_list_box_row_new();
    g_object_set_data_full(G_OBJECT(row), "certificate", g_object_ref(certificate), g_object_unref);
    g_object_set_data(G_OBJECT(row), "is_leaf", GINT_TO_POINTER(is_leaf));

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 8 + depth * 20);

    if (depth > 0) {
        GtkWidget* arrow = gtk_image_new_from_icon_name("arrow-turn-down-right-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(arrow), 16);
        gtk_widget_set_valign(arrow, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(box), arrow);
    }

    GtkWidget* icon = gtk_image_new_from_icon_name("application-certificate-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 16);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), icon);

    GtkWidget* labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(labels, TRUE);
    gtk_box_append(GTK_BOX(box), labels);

    GtkWidget* name = create_label(certificate_display_name(certificate).c_str(), GTK_ALIGN_START, true, false);
    limit_label_size(GTK_LABEL(name), 70, 1);
    gtk_box_append(GTK_BOX(labels), name);

    GtkWidget* role_label = create_label(role, GTK_ALIGN_START, true, false);
    gtk_widget_add_css_class(role_label, "dim-label");
    gtk_box_append(GTK_BOX(labels), role_label);

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    return row;
}

static void
certificate_info_dialog_set_certificate_info(
    CertificateInfoDialog* self,
    GTlsCertificate* certificate,
    GTlsCertificateFlags tls_errors,
    const char* uri)
{
    self->tls_errors = tls_errors;
    g_clear_pointer(&self->uri, g_free);
    self->uri = g_strdup(uri ? uri : "");
    clear_list_box(self->certificate_list);

    if (!certificate) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* label = create_label(_("No certificate"), GTK_ALIGN_START, true, false);
        gtk_widget_set_margin_top(label, 8);
        gtk_widget_set_margin_end(label, 8);
        gtk_widget_set_margin_bottom(label, 8);
        gtk_widget_set_margin_start(label, 8);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(self->certificate_list, row);
        gtk_list_box_select_row(self->certificate_list, GTK_LIST_BOX_ROW(row));
        certificate_info_dialog_populate_details(self, NULL, static_cast<GTlsCertificateFlags>(0));
        return;
    }

    std::vector<GTlsCertificate*> chain;
    for (GTlsCertificate* current = certificate; current && chain.size() < 16;) {
        chain.push_back(current);
        GTlsCertificate* issuer = g_tls_certificate_get_issuer(current);
        if (!issuer || issuer == current || g_tls_certificate_is_same(current, issuer)) {
            break;
        }
        current = issuer;
    }
    std::reverse(chain.begin(), chain.end());

    GtkWidget* leaf_row = NULL;
    for (size_t i = 0; i < chain.size(); ++i) {
        const bool is_leaf = i == chain.size() - 1;
        const char* role = is_leaf
            ? _("Server certificate")
            : (i == 0 ? _("Root certificate authority") : _("Intermediate certificate authority"));
        GtkWidget* row = create_certificate_row(chain[i], static_cast<int>(i), role, is_leaf);
        gtk_list_box_append(self->certificate_list, row);
        if (is_leaf) {
            leaf_row = row;
        }
    }

    if (leaf_row) {
        gtk_list_box_select_row(self->certificate_list, GTK_LIST_BOX_ROW(leaf_row));
    } else {
        certificate_info_dialog_populate_details(
            self,
            certificate,
            display_tls_errors_for_certificate(certificate, self->tls_errors, true)
        );
    }
}

AdwDialog*
certificate_info_dialog_new(GTlsCertificate* certificate, GTlsCertificateFlags tls_errors, const char* uri)
{
    auto dialog = CERTIFICATE_INFO_DIALOG(g_object_new(CERTIFICATE_INFO_DIALOG_TYPE, NULL));
    certificate_info_dialog_set_certificate_info(dialog, certificate, tls_errors, uri);
    return ADW_DIALOG(dialog);
}

static void
on_certificate_list_row_selected(GtkListBox* list_box, GtkListBoxRow* row, gpointer user_data)
{
    auto self = CERTIFICATE_INFO_DIALOG(user_data);
    if (!row) {
        return;
    }

    gpointer data = g_object_get_data(G_OBJECT(row), "certificate");
    auto certificate = data ? G_TLS_CERTIFICATE(data) : NULL;
    const bool is_leaf = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "is_leaf"));
    certificate_info_dialog_populate_details(
        self,
        certificate,
        display_tls_errors_for_certificate(certificate, self->tls_errors, is_leaf)
    );
}

static void
on_ok_button_clicked(GtkButton* button, gpointer user_data)
{
    auto win = CERTIFICATE_INFO_DIALOG(user_data);
    adw_dialog_close(ADW_DIALOG(win));
}

static void
certificate_info_dialog_finalize(GObject* object)
{
    auto self = CERTIFICATE_INFO_DIALOG(object);
    g_clear_pointer(&self->uri, g_free);

    GObjectClass* klass = G_OBJECT_CLASS(certificate_info_dialog_parent_class);
    klass->finalize(object);
}

static void
certificate_info_dialog_init(CertificateInfoDialog* win)
{
    win->uri = NULL;
    win->tls_errors = static_cast<GTlsCertificateFlags>(0);
    gtk_widget_init_template(GTK_WIDGET(win));
}

static void
certificate_info_dialog_class_init(CertificateInfoDialogClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = certificate_info_dialog_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/certificate_info_dialog.ui");

    gtk_widget_class_bind_template_child(widget_class, CertificateInfoDialog, title_label);
    gtk_widget_class_bind_template_child(widget_class, CertificateInfoDialog, certificate_list);
    gtk_widget_class_bind_template_child(widget_class, CertificateInfoDialog, details_box);

    gtk_widget_class_bind_template_callback(widget_class, on_certificate_list_row_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_ok_button_clicked);
}
