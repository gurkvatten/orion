#pragma once

#include <memory>
#include <string>
#include <functional>

#include <common/src/error.h>
#include <core/dto/profile_dto.h>
#include <core/dto/bookmark_dto.h>

namespace orion {

class BookmarkHtmlExporter {
public:
    static std::shared_ptr<BookmarkHtmlExporter> create();
    virtual ~BookmarkHtmlExporter() = default;

    virtual Error export_bookmarks_to_html(BookmarkItemDto root_item, std::string* out_text) = 0;
    virtual void set_escape_html_text_fn(std::function<std::string(const std::string&)> fn) = 0;
    virtual void set_escape_uri_fn(std::function<std::string(const std::string&)> fn) = 0;
};

} // namespace orion