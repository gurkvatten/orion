#pragma once
#include <../common/src/error.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace orion::sync {

class FileClient {
public:
    FileClient(const std::string& path) :
        m_path(std::move(path)) {}
    FileClient(const std::filesystem::path& path) :
        m_path(path) {}

    //squential read/write/exchange
    Error read(const std::string& collection, std::string& message);
    Error write(const std::string& collection, const std::string& message);
    Error exchange(const std::string& collection, std::string& buffer);

    Error rollback();
    void cleanup();

    ~FileClient() {
        cleanup();
    }

private:
    void assure_open(const std::string& collection, std::ios_base::openmode mode);

private:
    std::string m_collection;
    std::filesystem::path m_path;
    std::filebuf m_file;
    std::ios::openmode m_mode;
};

} //orion::sync
