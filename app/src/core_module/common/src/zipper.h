#pragma once

#include <memory>
#include <src/error.h>
#include <string>
#include <map>
#include <stdint.h>

namespace orion {

struct ZipperImpl;

class Zipper {
public:
    struct Node {
        bool is_dir = false;
        std::vector<uint8_t> file_data;                        // only valid if is_dir is false
        std::map<std::string, std::unique_ptr<Node>> children; // only valid if is_dir is true
    };

    static std::shared_ptr<Zipper> create();
    void close();
    ~Zipper();

    Error extract_to_memory(const std::string& zip_path, Node* root);

    ZipperImpl* impl() const { return m_impl.get(); }

private:
    Zipper() = default; // private constructor to enforce use of create()
    Zipper(const Zipper&) = delete;
    Zipper& operator=(const Zipper&) = delete;
    Zipper(Zipper&&) = delete;
    Zipper& operator=(Zipper&&) = delete;

private:
    std::unique_ptr<ZipperImpl> m_impl;
};

} // namespace orion