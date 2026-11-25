#include <src/dto/totp.h>

namespace orion {

std::string
to_string(HashAlgo algo)
{
    switch (algo) {
        case HashAlgo::sha1: return "SHA1";
        case HashAlgo::sha256: return "SHA256";
        default: return "<unknown-algo>";
    }
}

} // namespace orion