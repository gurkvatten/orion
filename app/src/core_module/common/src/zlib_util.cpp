#include "zlib_util.h"
#include <zlib.h>

namespace orion {

std::string
GZip::compress(const std::string in, Error& err)
{
    err = Error::ok();

    uLongf compSize = compressBound(in.size());
    std::vector<unsigned char> output;
    m_buffer.resize(static_cast<size_t>(compSize));

    int error = ::compress(m_buffer.data(), &compSize, reinterpret_cast<const Bytef*>(in.data()), in.size());
    if (error != Z_OK) {
        err = MAKE_ERROR(ErrorCode::compression, "zlib compress failed with code: %i", error);
        return "";
    }

    return std::string(reinterpret_cast<char*>(m_buffer.data()), compSize);
}

std::string
GZip::decompress(const std::string in, Error& err)
{
    err = Error::ok();

    uLongf outSize = in.size() * 4;
    int ret;

    int n = 0; //sanity chanck
    do {
        m_buffer.resize(static_cast<size_t>(outSize));
        if ((ret = ::uncompress(m_buffer.data(), &outSize, reinterpret_cast<const Bytef*>(in.data()), in.size())) != Z_BUF_ERROR) {
            break;
        }
        outSize *= 2;
    } while (in.size() / outSize < 24); //don't think that we would achieve 1:16 compression rate

    if (ret != Z_OK) {
        err = MAKE_ERROR(ErrorCode::compression, "zlib decompress failed with code: %i", ret);
        return "";
    }

    return std::string(reinterpret_cast<char*>(m_buffer.data()), outSize);
}

} // namespace orion
