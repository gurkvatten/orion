#include "bookmark_html_importer.h"

#include <memory>
#include <map>
#include <format>
#include <regex>

#include <common/src/string_util.h>

namespace orion {

class BookmarkHtmlImporterImpl : public BookmarkHtmlImporter {
public:
    ~BookmarkHtmlImporterImpl() override = default;
    Error import_bookmarks_from_html(const std::string& html, BookmarkItemDto* out_root_item) override;

private:
    std::function<std::string(const std::string&)> m_unescape_html_text_fn;
    std::function<std::string(const std::string&)> m_unescape_uri_fn;

    friend std::shared_ptr<BookmarkHtmlImporter> BookmarkHtmlImporter::create();
};

std::shared_ptr<BookmarkHtmlImporter>
BookmarkHtmlImporter::create()
{
    auto importer = std::make_shared<BookmarkHtmlImporterImpl>();

    importer->m_unescape_html_text_fn = [](const std::string& text) {
        std::string r = text;
        orion::str::replace_all(&r, "&lt;", "<");
        orion::str::replace_all(&r, "&gt;", ">");
        return r;
    };

    importer->m_unescape_uri_fn = [](const std::string& uri) {
        std::string r = uri;
        orion::str::replace_all(&r, "%22", "\"");
        return r;
    };

    return importer;
}

Error
BookmarkHtmlImporterImpl::import_bookmarks_from_html(
    const std::string& html, BookmarkItemDto* out_root_item)
{
    RAISE_CHECK(out_root_item);

    // <DT>                      Definition Term (begins a new bookmark item or folder item)
    // <DL><p>                   Definition List (starts a container for folder's contents)
    // <H3>                      Folder title
    // ADD_DATE                  Unix timestamp of when the bookmark or folder was created
    // LAST_MODIFIED             Unix timestamp of when the bookmark or folder was last modified
    // PERSONAL_TOOLBAR_FOLDER   Bookmarks Toolbar (folder)
    // PERSONAL_UNFILED_FOLDER   Other Bookmarks (folder)
    
    const std::regex folder_name_regex(R"(<DT>[^<]*<H3[^>]*>([^<]*)</H3>)");
    const std::regex bookmark_regex(R"zzz(<DT>[^<]*<A\s+[^>]*HREF="([^"]*)"[^>]*>([^<]*)</A>)zzz");

    std::deque<std::string> lines = orion::str::split(html, '\n');
    int line_num = 0;
    bool header_found = false;
    std::map<BookmarkItemDto*, BookmarkItemDto*> parent_map; // child -> parent
    BookmarkItemDto* current_item = out_root_item;

    current_item->type = BookmarkItemType::folder; // root is a (virtual) folder
    current_item->folder = BookmarkFolderDto{
        .name = "",
        .type = BookmarkFolderType::normal,
    };

    for (auto& line : lines) {
        line_num++;
        orion::str::trim(&line);

        if (line.empty()) {
            continue;
        }

        if (line == "<!DOCTYPE NETSCAPE-Bookmark-file-1>") {
            header_found = true;
            continue;
        }

        if (!header_found) {
            continue;
        }

        // <HTML> tag may be present but we do not require it

        std::smatch match{};

        if (std::regex_search(line, match, folder_name_regex)) {
            if (match.size() != 2) {
                RAISE(ErrorCode::assert, "unexpected regex match size %zu instead of %zu at line %d",
                    match.size(), 2, line_num);
            }

            std::string name = m_unescape_html_text_fn(match[1].str());

            current_item->children.push_back(BookmarkItemDto{
                .folder = BookmarkFolderDto{
                    .name = name,
                },
                .type = BookmarkItemType::folder,
            });
            auto parent = current_item;
            current_item = &current_item->children.back();
            parent_map[current_item] = parent;
            continue;
        }

        if (line == "<DL><p>" || line == "<DL>") {
            continue;
        }

        if (std::regex_search(line, match, bookmark_regex)) {
            if (match.size() != 3) {
                RAISE(ErrorCode::assert, "unexpected regex match size %zu instead of %zu at line %d",
                    match.size(), 3, line_num);
            }

            std::string url = m_unescape_uri_fn(match[1].str());
            std::string name = m_unescape_html_text_fn(match[2].str());

            current_item->children.push_back(BookmarkItemDto{
                .bookmark = BookmarkDto{
                    .name = name,
                    .address = url,
                },
                .type = BookmarkItemType::bookmark,
            });
            continue;
        }

        if (line == "</DL><p>" || line == "</DL>") {
            // Current folder item is finished

            // This check would break Chrome/Firefox bookmark import:
            // if (current_item == out_root_item) {
            //     RAISE(ErrorCode::assert, "unexpected closing </DL> tag at line %d", line_num);
            // }

            // Move up to the parent folder
            auto it = parent_map.find(current_item);
            if (it == parent_map.end()) {
                // This check would break Chrome/Firefox bookmark import:
                //RAISE(ErrorCode::assert, "parent folder not found for current folder at line %d", line_num);
                current_item = out_root_item; 
            } else {
                current_item = it->second;
            }
            continue;
        }

        // Ignore </HTML> possibly marking the end of the file
    }
    SUCCEED();
}

} // namespace orion