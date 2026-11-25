#include "bookmark_html_exporter.h"

#include <memory>
#include <format>

#include <common/src/string_util.h>

namespace orion {

class BookmarkHtmlExporterImpl : public BookmarkHtmlExporter {
public:
    ~BookmarkHtmlExporterImpl() override = default;
    Error export_bookmarks_to_html(BookmarkItemDto root_item, std::string* out_text) override;
    void set_escape_html_text_fn(std::function<std::string(const std::string&)> fn) override;
    void set_escape_uri_fn(std::function<std::string(const std::string&)> fn) override;

private:
    void append_line(std::string* text, int indent_lvl, const std::string& line);
    Error export_folder_recursive(std::string* out_text, int indent_lvl, BookmarkItemDto item);

private:
    std::function<std::string(const std::string&)> m_escape_html_text_fn;
    std::function<std::string(const std::string&)> m_escape_uri_fn;
};

std::shared_ptr<BookmarkHtmlExporter>
BookmarkHtmlExporter::create()
{
    auto exporter = std::make_shared<BookmarkHtmlExporterImpl>();

    exporter->set_escape_html_text_fn([](const std::string& text) {
        std::string r = text;
        orion::str::replace_all(&r, "<", "&lt;");
        orion::str::replace_all(&r, ">", "&gt;");
        return r;
    });

    exporter->set_escape_uri_fn([](const std::string& uri) {
        std::string r = uri;
        orion::str::replace_all(&r, "\"", "%22");
        return r;
    });

    return exporter;
}

void
BookmarkHtmlExporterImpl::set_escape_html_text_fn(std::function<std::string(const std::string&)> fn)
{
    m_escape_html_text_fn = std::move(fn);
}

void
BookmarkHtmlExporterImpl::set_escape_uri_fn(std::function<std::string(const std::string&)> fn)
{
    m_escape_uri_fn = std::move(fn);
}

// Mac Orion uses a different format than Mozilla Firefox, use Mac Orion's format here
Error
BookmarkHtmlExporterImpl::export_bookmarks_to_html(BookmarkItemDto root_item, std::string* out_text)
{
    RAISE_CHECK(out_text);

    // <DT>                      Definition Term (begins a new bookmark item or folder item)
    // <DL><p>                   Definition List (starts a container for folder's contents)
    // <H3>                      Folder title
    // ADD_DATE                  Unix timestamp of when the bookmark or folder was created
    // LAST_MODIFIED             Unix timestamp of when the bookmark or folder was last modified
    // PERSONAL_TOOLBAR_FOLDER   Bookmarks Toolbar (folder)
    // PERSONAL_UNFILED_FOLDER   Other Bookmarks (folder)

    append_line(out_text, 0, "<!DOCTYPE NETSCAPE-Bookmark-file-1>");
    append_line(out_text, 1, "<HTML>");
    append_line(out_text, 1, R"(<META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=UTF-8">)");
    append_line(out_text, 1, "<Title>Bookmarks</Title>");
    append_line(out_text, 1, "<H1>Bookmarks</H1><DL><p>");

    VERIFY(export_folder_recursive(out_text, 1, root_item));

    append_line(out_text, 0, "</HTML>");
    SUCCEED();
}

Error
BookmarkHtmlExporterImpl::export_folder_recursive(std::string* out_text, int indent_lvl, BookmarkItemDto item)
{
    if (!m_escape_html_text_fn) {
        RAISE(ErrorCode::invalid_state, "escape_html_text_fn is not set");
    }

    if (!m_escape_uri_fn) {
        RAISE(ErrorCode::invalid_state, "escape_uri_fn is not set");
    }

    if (item.type == BookmarkItemType::folder) {
        if (item.folder.id == BookmarkFolderId(0)) {
            // This is the root (virtual) folder, process its children directly
            for (const auto& child : item.children) {
                VERIFY(export_folder_recursive(out_text, indent_lvl, child));
            }
        } else {
            // escape special characters in folder and bookmark names (e.g., &, <, >)
            std::string name = m_escape_html_text_fn(item.folder.name);
            append_line(out_text, indent_lvl, std::format("<DT><H3 FOLDED>{}</H3>", name));
            append_line(out_text, indent_lvl, "<DL><p>");
            for (const auto& child : item.children) {
                VERIFY(export_folder_recursive(out_text, indent_lvl + 1, child));
            }
            append_line(out_text, indent_lvl, "</DL><p>");
        }
    } else if (item.type == BookmarkItemType::bookmark) {
        std::string name = m_escape_html_text_fn(item.bookmark.name);
        // escape special characters in bookmark URLs (e.g., &, <, >, ", ')
        std::string address = m_escape_uri_fn(item.bookmark.address);
        std::string line = std::format(R"(<DT><A HREF="{}">{}</A>)", address, name);
        append_line(out_text, indent_lvl, line);
    } else {
        RAISE(ErrorCode::argument, "invalid bookmark item type: %d", static_cast<int>(item.type));
    }
    SUCCEED();
}

void
BookmarkHtmlExporterImpl::append_line(std::string* text, int indent_lvl, const std::string& line)
{
    for (int i = 0; i < indent_lvl; i++) {
        text->append("\t"); // tab (used on Mac Orion)
    }
    text->append(line);
    text->append("\n"); // line feed (used on Mac Orion)
}

} // namespace orion