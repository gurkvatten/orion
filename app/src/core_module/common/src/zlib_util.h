#include "error.h" 

namespace orion {

/*
    !NOTE: this class isn't a tread safe and hnece every thread must have it's own instance
*/
class GZip {

public:
    GZip() = default;
    ~GZip() = default;

/*these method use default mem allocator nd compression setting of the zlib
*/
    std::string compress(const std::string in, Error& err);
    std::string decompress(const std::string in, Error& err);

    void cleanup() {
        m_buffer.clear();
        m_buffer.shrink_to_fit();
    }

private:
    std::vector<unsigned char> m_buffer; 
}; 

}//orion