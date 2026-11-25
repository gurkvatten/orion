#define ENABLE_CRYPTO_TEST !NDEBUG && 1
#define ENABLE_VERBOSE_LOGS 0

#include <src/crypto_test.h>
#include <src/crypto.h>
#include <src/log.h>
#include <src/error.h>
#include <src/dispatch.h>

namespace orion::crypto {

namespace {
    struct Case {
        std::string input;
        std::string expected_hex;
    };
}

[[maybe_unused]]
static Error
test_sha1()
{
    Case cases[] = {
        {"abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
        {"hello", "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d"},
        {"", "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
        {"The quick brown fox jumps over the lazy dog", "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"},
        {"The quick brown fox jumps over the lazy dog.", "408d94384216f890ff7a0c3528e8bed1e0b01621"},
    };
    for (const auto& test_case : cases) {
        uint8_t hash[20];
        sha1(reinterpret_cast<const uint8_t*>(test_case.input.data()), test_case.input.size(), hash);

        char hash_hex[41];
        for (size_t i = 0; i < 20; ++i) {
            sprintf(&hash_hex[i * 2], "%02x", hash[i]);
        }
        hash_hex[40] = '\0';

        std::string hash_hex_str(hash_hex);
        if (hash_hex_str != test_case.expected_hex) {
            RAISE(ErrorCode::assert, "crypto_test: test_sha1 failed for input '%s': expected %s, got %s",
                test_case.input.c_str(),
                test_case.expected_hex.c_str(),
                hash_hex_str.c_str()
            );
        } else {
            log_verbose("crypto_test: test_sha1 passed for input '%s'", test_case.input.c_str());
        }
    }
    SUCCEED();
}

[[maybe_unused]]
static Error
test_sha256()
{
    Case cases[] = {
        {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"hello", "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"},
        {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"The quick brown fox jumps over the lazy dog", "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"},
        {"The quick brown fox jumps over the lazy dog.", "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c"},
    };
    for (const auto& test_case : cases) {
        uint8_t hash[32];
        sha256(reinterpret_cast<const uint8_t*>(test_case.input.data()), test_case.input.size(), hash);

        char hash_hex[65];
        for (size_t i = 0; i < 32; ++i) {
            sprintf(&hash_hex[i * 2], "%02x", hash[i]);
        }
        hash_hex[64] = '\0';

        std::string hash_hex_str(hash_hex);
        if (hash_hex_str != test_case.expected_hex) {
            RAISE(ErrorCode::assert, "crypto_test: test_sha256 failed for input '%s': expected %s, got %s",
                test_case.input.c_str(),
                test_case.expected_hex.c_str(),
                hash_hex_str.c_str()
            );
        } else {
            log_verbose("crypto_test: test_sha256 passed for input '%s'", test_case.input.c_str());
        }
    }
    SUCCEED();
}

static Error
test_md5_str()
{
    Case cases[] = {
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"hello", "5d41402abc4b2a76b9719d911017c592"},
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"The quick brown fox jumps over the lazy dog", "9e107d9d372bb6826bd81d3542a419d6"},
        {"The quick brown fox jumps over the lazy dog.", "e4d909c290d0fb1ca068ffaddf22cbd0"},
    };
    for (const auto& test_case : cases) {
        std::string hash_hex = md5_str(test_case.input);

        std::string hash_hex_str(hash_hex);
        if (hash_hex_str != test_case.expected_hex) {
            RAISE(ErrorCode::assert, "crypto_test: test_md5_str failed for input '%s': expected %s, got %s",
                test_case.input.c_str(),
                test_case.expected_hex.c_str(),
                hash_hex_str.c_str()
            );
        } else {
            log_verbose("crypto_test: test_md5_str passed for input '%s'", test_case.input.c_str());
        }
    }
    SUCCEED();
}

[[maybe_unused]]
static Error
test_totp()
{
    struct TotpCase {
        std::string b32_secret;
        int digits;
        uint64_t unix_time;
        int period;
        HashAlgo algo;
        std::string expected_code;
    };

    TotpCase cases[] = {
        {"as", 6, 1764688307, 30, HashAlgo::sha1, "512555"}, // leftover bits should be ignored, not rejected
        {"JBSWY3DPEHPK3PXP", 6, 1764682203, 30, HashAlgo::sha1, "719872"},
        {"JBSWY3DPEHPK3PXP", 6, 1764682480, 30, HashAlgo::sha256, "863830"},
        {"GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", 8, 1764682594, 30, HashAlgo::sha1, "85952604"},
        {"GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", 8, 1764682909, 30, HashAlgo::sha256, "63127267"},
    };

    for (const auto& test_case : cases) {
        std::string code;
        VERIFY(totp(
            test_case.b32_secret.c_str(),
            test_case.digits,
            test_case.unix_time,
            test_case.period,
            test_case.algo,
            &code
        ));

        if (code != test_case.expected_code) {
            RAISE(ErrorCode::assert, "crypto_test: test_totp failed for secret '%s' algo %s at time %llu: expected %s, got %s",
                test_case.b32_secret.c_str(),
                to_string(test_case.algo).c_str(),
                static_cast<unsigned long long>(test_case.unix_time),
                test_case.expected_code.c_str(),
                code.c_str()
            );
        } else {
            log_verbose("crypto_test: test_totp passed for secret '%s' at time %llu",
                test_case.b32_secret.c_str(),
                static_cast<unsigned long long>(test_case.unix_time)
            );
        }
    }

    SUCCEED();
}

void
crypto_test()
{
#if ENABLE_CRYPTO_TEST
    dispatch(on_disk_thread(), []{
        log_verbose("running crypto test");
        VERIFY_LOG(test_sha1());
        VERIFY_LOG(test_sha256());
        VERIFY_LOG(test_md5_str());
        VERIFY_LOG(test_totp());
    });
#endif
}
    
} // namespace orion::crypto