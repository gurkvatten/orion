#define ENABLE_VERBOSE_LOGS 0

#include <src/zipper.h>
#include <zip.h>
#include <src/defer.h>
#include <src/memory.h>
#include <cstdarg>
#include <cstring>
#include <iostream>

static size_t
offset_for_file(const std::string& filepath)
{
    UNIQUE_PTR(FILE, fclose) f (fopen(filepath.c_str(), "rb"));
    if (!f) return 0;
    char magic[4];
    if (fread(magic, 1, 4, f.get()) != 4)
        return 0; // error

    if (memcmp(magic, "Cr24", 4) != 0)
        return 0;

    uint32_t version;
    if (fread(&version, 4, 1, f.get()) != 1)
        return 0; // error

    if (version == 2) {
        uint32_t pubkey_len, sig_len;
        if (fread(&pubkey_len, 4, 1, f.get()) != 1)
            return 0; // error

        if (fread(&sig_len, 4, 1, f.get()) != 1)
            return 0; // error

        return 16 + pubkey_len + sig_len; // header size
    } else if (version == 3) {
        uint32_t header_len;
        if (fread(&header_len, 4, 1, f.get()) != 1)
            return 0; // error
            
        return 12 + header_len;
    } else {
        return 0;
    }
    return 0;
}

namespace orion {

struct ZipperImpl {
};

std::shared_ptr<Zipper>
Zipper::create()
{
    auto self = std::shared_ptr<Zipper>(new Zipper);
    log_verbose("Zipper::create");
    self->m_impl = std::make_unique<ZipperImpl>();
    return self;
}

void
Zipper::close()
{
    log_verbose("Zipper::close");
}

Zipper::~Zipper()
{
    log_verbose("Zipper::~Zipper");
    close();
}

#define RAISE_ZIP_ERROR(err_code, format, ...) \
    do { \
        zip_error_t error__{}; /* the var name can be made more distinct using __LINE__ trick */ \
        zip_error_init_with_code(&error__, err_code); \
        defer([&error__]{ zip_error_fini(&error__); }); \
        RAISE(ErrorCode::io, \
            (std::string("libzip failed: ") + sprintf_to_string(format, ##__VA_ARGS__) + ": %s").c_str(), \
            zip_error_strerror(&error__) \
        ); \
    } while (0)

#define ZIP_ERROR(err_code, format, ...) \
    do { \
        zip_error_t error__{}; /* the var name can be made more distinct using __LINE__ trick */ \
        zip_error_init_with_code(&error__, err_code); \
        defer([&error__]{ zip_error_fini(&error__); }); \
        log_error((std::string("libzip failed: ") + sprintf_to_string(format, ##__VA_ARGS__) + ": %s").c_str(), \
            zip_error_strerror(&error__) \
        ); \
    } while (0)

Error
create_error(zip_error_t* error, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    auto err = MAKE_ERROR(ErrorCode::io, (sprintf_to_string(format, args) + ": " + zip_error_strerror(error)).c_str());
    va_end(args);
    return err;
}

void
insert_path(Zipper::Node* root, const std::string& path, std::vector<uint8_t> data, bool is_dir)
{
    std::stringstream ss(path);
    std::string part;
    Zipper::Node* current = root;

    std::vector<std::string> parts;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }

    for (size_t i = 0; i < parts.size(); i++) {
        const std::string& name = parts[i];
        bool last = (i == parts.size() - 1);

        if (current->children.find(name) == current->children.end()) {
            current->children[name] = std::make_unique<Zipper::Node>();
            current->children[name]->is_dir = !last ? true : is_dir;
        }

        current = current->children[name].get();

        if (last && !is_dir) {
            current->file_data = std::move(data);
        }
    }
}

// Recursively print tree
void
print_tree(const Zipper::Node* node, const std::string& prefix = "")
{
    for (auto& [name, child] : node->children) {
        if (child->is_dir) {
            std::cout << prefix << "[DIR] " << name << "\n";
            print_tree(child.get(), prefix + "  ");
        } else {
            std::cout << prefix << "[FILE] " << name 
                << " (" << child->file_data.size() << " bytes)\n";
        }
    }
}

Error
Zipper::extract_to_memory(const std::string& zip_path, Node* root)
{
    log_verbose("Zipper::extract_to_memory: %s", zip_path.c_str());
    zip_error_t ziperror;
    size_t offset = offset_for_file(zip_path);
    zip_source_t* source = zip_source_file_create(zip_path.c_str(), offset, -1, &ziperror);

    zip_t* archive = zip_open_from_source(source, 0, &ziperror);
    if (!archive) {
        zip_source_free(source);
        RAISE(ErrorCode::io, "failed to open archive: %s", zip_path.c_str());
    }

    defer([&archive, zip_path]{
        int err = zip_close(archive);
        if (err != 0) {
            ZIP_ERROR(err, "failed to close archive: %s", zip_path.c_str());
        }
    });

    root->is_dir = true;
    int file_count = 0;

    zip_int64_t num_entries = zip_get_num_entries(archive, 0);
    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(num_entries); i++) {
        struct zip_stat st{};
        zip_stat_init(&st);
        if (zip_stat_index(archive, i, 0, &st) != 0) {
            return create_error(zip_get_error(archive), "failed to stat entry at index %zu", i);
        }

        std::string name = st.name;

        if (name.back() == '/') {
            insert_path(root, name, {}, true);
            continue;
        }

        zip_file_t* file = zip_fopen_index(archive, i, 0);
        if (!file) {
            return create_error(zip_get_error(archive), "failed to open file: %s", name.c_str());
        }

        std::vector<uint8_t> buffer(st.size);
        zip_int64_t bytes_read = zip_fread(file, buffer.data(), buffer.size());
        int err = zip_fclose(file);
        if (err != 0) {
            ZIP_ERROR(err, "failed to close file: %s", zip_path.c_str());
        }

        if (bytes_read < 0 || static_cast<zip_uint64_t>(bytes_read) != st.size) {
            RAISE(ErrorCode::io, "failed to read file from zip: %s (expected %zu bytes, got %zd bytes)", name.c_str(), st.size, bytes_read);
        }

        insert_path(root, name, std::move(buffer), false);
        file_count++;
    }

    //print_tree(root.get());
    log_verbose("Zipper::extract_to_memory: %s (files: %d)", zip_path.c_str(), file_count);

    SUCCEED();
}

} // namespace orion
