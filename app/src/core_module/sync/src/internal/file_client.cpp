#include "file_client.h"
#include <../common/src/file_system.h>
#include <stdexcept>

namespace fs = std::filesystem;
namespace orion::sync {

const std::string file_client_ext = ".osnc";
const std::string file_client_bckp_ext = ".osnc.bckp";

static const char* locale[] = {"en_US.UTF-8", "en_US.UTF8", "C.UTF-8", "UTF-8", ""}; //try UTF-8 locales first, then fallback to default

static void
set_locale_utf8(std::filebuf& file)
{
    // For C++20, use standard locale instead of deprecated codecvt_utf8 which still includes the header nevertheless
    int i = 0;
    do {
        try {
            file.pubimbue(std::locale(locale[i]));
            return;
        } catch (const std::runtime_error&) {
            // Try the next locale in the list
            ++i;
        }
    } while (*locale[i] != '\0');
     // If we have tried all locales and still failed, use the default locale
    file.pubimbue(std::locale());
};

static fs::path
full_file_path(const fs::path& path, const std::string& collection)
{
    return (path / collection).replace_extension(file_client_ext);
};

void
FileClient::assure_open(const std::string& collection, std::ios_base::openmode mode)
{ //eitewr in or out
    //we never open file for read and write simultaneously. If we do then it's en error
    //e.g. the first call to either read or write method determines the mode. any subsequent
    //method has to be the same as the 1st one else the op fails.
    if (!m_file.is_open()) {
        if (!m_collection.empty()) {
            throw std::logic_error("FileClient is in an invalid state: file isn't open but collection is set to " + m_collection);
        }
        if ((mode & (std::ios_base::in|std::ios_base::out)) == (std::ios_base::in|std::ios_base::out)) {
            throw std::invalid_argument("The file at path " + m_path.string() + " cannot be opened for both read and write at the same time");
        }
        std::error_code err;
        fs::path file_name = full_file_path(m_path, collection);
        if (!(fs::exists(m_path, err) || fs::create_directories(m_path, err))) {
            throw std::runtime_error("Couild not either check or create directory " + m_path.string() + " error code: " + err.message());
        } else {
            if (mode & std::ios_base::out) {
                if (fs::is_regular_file(file_name)) {
                    //rename the file first to keep the backup before opening it for write, ignore any error
                    fs::path file_bckp = full_file_path(m_path, collection).replace_extension(file_client_bckp_ext);
                    fs::remove(file_bckp); //just in case
                    fs::rename(file_name, file_bckp); //let it throw if it cannot rename
                }
                // and create a new file for write
                std::ofstream(file_name, std::ios::out|std::ios::app).close();
            }
            if (m_file.open(file_name, mode) == NULL) {
                throw std::runtime_error("Couild not open sync file at path " + m_path.string() + ", in mode " + (mode & std::ios_base::in ? "in" : "out"));
            }
            set_locale_utf8(m_file); //call this befoe we open stream?
            m_collection = collection;
            m_mode = mode;
        }
    } else if (!m_collection.empty() && m_collection != collection) {
        throw std::invalid_argument("The file at path " + m_path.string() + " is alread been open for a collection " + m_collection + ". Cannot re-open it for " + collection);
    } else if (m_mode != mode) {
        throw std::invalid_argument("The file at path " + m_path.string() + " is alread been open in mode " + (m_mode & std::ios_base::in ? "in" : "out")
            + ". Cannot re-open in mode " + (mode & std::ios_base::in ? "in" : "out"));
    }
};

Error
FileClient::read(const std::string& collection, std::string& message)
{
    assure_open(collection, std::ios::in);

    //throws exception on read error.
    char c;
    while (m_file.sgetc() != EOF && (c = m_file.sbumpc()) != '\n') {
        message.push_back(c);
    }
    SUCCEED();
};

Error
FileClient::write(const std::string& collection, const std::string& message)
{
    assure_open(collection, std::ios::out/*|std::ios::trunc we always create a new file since we'r doing backuo*/);

    //throws exception on write error
    std::streamsize n = m_file.sputn(message.c_str(), message.size());
    n += m_file.sputc('\n');
    if (n < static_cast<std::streamsize>(message.size() + 1)) {
        return MAKE_ERROR(ErrorCode::io, "Failed to write the whole message to the file at path %s", m_path.string().c_str());
    }
    SUCCEED();
};

Error
FileClient::exchange(const std::string& collection, std::string& buffer)
{
    RAISE(ErrorCode::not_implemented, "Synch server interface exchange isn't implemented yet");
};

static void
close(std::filebuf& file)
{
    if (file.is_open()) {
        file.pubsync();
        file.close();
    }
};

Error
FileClient::rollback()
{
    //revert the original backup ,if any.
    if (m_collection.empty()) {
        //we havent even done anything yet
        SUCCEED();
    }
    if (m_mode & std::ios::out) { //only do rollabck if we are writing
        fs::path file_name = full_file_path(m_path, m_collection);
        fs::path file_bckp = full_file_path(m_path, m_collection).replace_extension(file_client_bckp_ext);
        //if the file_bckp doesn't exist it means we didn't have any file when we started writing => delete the file in anycase
        close(m_file);
        fs::remove(file_name); //just in case
        if (fs::is_regular_file(file_bckp)) {
            fs::rename(file_bckp, file_name);
        } //otherwise leave what's there since there's nothing to rollback to
    }
    SUCCEED();
}

void
FileClient::cleanup()
{
    close(m_file);
    //TODO should we leave the previous backup file untouched so that we've always got one previous backup?
    if (!m_collection.empty()) {
        fs::path file_bckp = full_file_path(m_path, m_collection).replace_extension(file_client_bckp_ext);
        fs::remove(file_bckp);
    }
    m_collection.clear();
}

} //orion::sync