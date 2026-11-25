#pragma once

#include <stdint.h>
#include <stddef.h>

#include <src/error.h>
#include <src/dto/totp.h>

namespace orion::crypto {

void sha1(const uint8_t* msg, size_t len, uint8_t out[20]);
void sha256(const uint8_t* msg, size_t len, uint8_t out[32]);
void hmac_sha1(const uint8_t* key, size_t klen, const uint8_t* msg, size_t mlen, uint8_t out[20]);
void hmac_sha256(const uint8_t* key, size_t klen, const uint8_t* msg, size_t mlen, uint8_t out[32]);
// Returns decoded length, or -1 on error. Ignores leftover bits instead of rejecting them.
int base32_decode_relaxed(const char* in, uint8_t* out, size_t out_max);
Error totp(const char* b32_secret, int digits, uint64_t unix_time, int period, HashAlgo algo, std::string* out_code);
// String to string conversion
std::string md5_str(const std::string& input);
std::string sha256_str(const std::string& input);

} // namespace orion::crypto