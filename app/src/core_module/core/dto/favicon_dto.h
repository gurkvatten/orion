#pragma once

#include <string>
#include <vector>
#include <common/src/dto/dto_member.h>
#include <common/src/error.h>

namespace orion {

struct FaviconDto {
    DTO_MEMBER(icon_url, std::string);
    DTO_MEMBER(data, std::vector<uint8_t>);
};

struct SetFaviconQuery {
    DTO_MEMBER(page_url, std::string);
    DTO_MEMBER(icon_url, std::string);
    DTO_MEMBER(data, std::vector<uint8_t>);
};

struct GetFaviconReply {
    DTO_MEMBER(icon_url, std::string);
    DTO_MEMBER(data, std::vector<uint8_t>);
    DTO_MEMBER(found, bool, false);
    DTO_MEMBER(expired, bool, false); // true if stamp is older than 4 days (caller should re-fetch)
    DTO_MEMBER(err, Error);
};

} // namespace orion
