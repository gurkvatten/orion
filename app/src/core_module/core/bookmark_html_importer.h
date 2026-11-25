#pragma once

#include <memory>
#include <string>
#include <functional>

#include <common/src/error.h>
#include <core/dto/profile_dto.h>
#include <core/dto/bookmark_dto.h>

namespace orion {

class BookmarkHtmlImporter {
public:
    static std::shared_ptr<BookmarkHtmlImporter> create();
    virtual ~BookmarkHtmlImporter() = default;
    virtual Error import_bookmarks_from_html(const std::string& html, BookmarkItemDto* out_root_item) = 0;
};

} // namespace orion