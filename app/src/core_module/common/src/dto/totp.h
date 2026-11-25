#pragma once

#include <string>
#include <cstdint>

#include <src/dto/dto_member.h>

namespace orion {

enum class HashAlgo {
    sha1,
    sha256,
};

struct GenerateTotpVerificationCodeQuery {
    DTO_MEMBER(verification_key, std::string);
    DTO_MEMBER(algo, HashAlgo);
    DTO_MEMBER(digits, int, 6);
    DTO_MEMBER(period_seconds, int, 30);
    DTO_MEMBER(timestamp, uint64_t, 0); // if 0, current time is used
};

std::string to_string(HashAlgo algo);

} // namespace orion