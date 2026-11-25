#include <src/crypto.h>

#include <cstring>
#include <iomanip>
#include <sstream>

namespace orion::crypto {

/* 32-bit left rotate - should work on every compiler */
static inline uint32_t
rotl32(uint32_t a, uint32_t b)
{
    return (a << b) | (a >> (32 - b));
}

void
sha1(const uint8_t* msg, size_t len, uint8_t out[20])
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint64_t bitlen = len * 8;
    uint8_t buf[128] = {0};
    size_t pos = 0;

    for (size_t i = 0; i < len; ++i) {
        buf[pos++] = msg[i];
        if (pos == 64) {
            /* process block */
            uint32_t w[80];
            for (int j = 0; j < 16; ++j)
                w[j] = (uint32_t)buf[j*4] << 24 | buf[j*4+1] << 16 | buf[j*4+2] << 8 | buf[j*4+3];
            for (int j = 16; j < 80; ++j)
                w[j] = rotl32(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);

            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
            for (int j = 0; j < 80; ++j) {
                uint32_t f, k;
                if (j < 20)      { f = (b & c) | (~b & d); k = 0x5A827999; }
                else if (j < 40) { f = b ^ c ^ d;          k = 0x6ED9EBA1; }
                else if (j < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                else             { f = b ^ c ^ d;          k = 0xCA62C1D6; }
                uint32_t temp = rotl32(a,5) + f + e + k + w[j];
                e = d; d = c; c = rotl32(b,30); b = a; a = temp;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
            pos = 0;
        }
    }

    buf[pos++] = 0x80;
    if (pos > 56) {
        memset(buf + pos, 0, 64 - pos);
        /* process padding block */
        uint32_t w[80] = {0};
        for (int j = 0; j < 16; ++j)
            w[j] = (uint32_t)buf[j*4] << 24 | buf[j*4+1] << 16 | buf[j*4+2] << 8 | buf[j*4+3];
        for (int j = 16; j < 80; ++j) w[j] = rotl32(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int j = 0; j < 80; ++j) {
            uint32_t f, k;
            if (j < 20)      { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (j < 40) { f = b ^ c ^ d;          k = 0x6ED9EBA1; }
            else if (j < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;          k = 0xCA62C1D6; }
            uint32_t temp = rotl32(a,5) + f + e + k + w[j];
            e = d; d = c; c = rotl32(b,30); b = a; a = temp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
        pos = 0;
    }
    memset(buf + pos, 0, 56 - pos);
    for (int i = 0; i < 8; ++i) buf[56 + i] = (uint8_t)(bitlen >> (56 - i*8));
    /* final block */
    uint32_t w[80] = {0};
    for (int j = 0; j < 16; ++j)
        w[j] = (uint32_t)buf[j*4] << 24 | buf[j*4+1] << 16 | buf[j*4+2] << 8 | buf[j*4+3];
    for (int j = 16; j < 80; ++j) w[j] = rotl32(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int j = 0; j < 80; ++j) {
        uint32_t f, k;
        if (j < 20)      { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (j < 40) { f = b ^ c ^ d;          k = 0x6ED9EBA1; }
        else if (j < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;          k = 0xCA62C1D6; }
        uint32_t temp = rotl32(a,5) + f + e + k + w[j];
        e = d; d = c; c = rotl32(b,30); b = a; a = temp;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;

    for (int i = 0; i < 5; ++i) {
        out[i*4]   = h[i] >> 24;
        out[i*4+1] = h[i] >> 16;
        out[i*4+2] = h[i] >>  8;
        out[i*4+3] = h[i];
    }
}

void
sha256(const uint8_t* data, size_t len, uint8_t out[32])
{
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint64_t bitlen = len * 8;

    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    uint8_t block[64];
    size_t offset = 0;

    #define PROCESS_BLOCK() { \
        uint32_t w[64]; \
        for (int i=0;i<16;i++) \
            w[i] = (uint32_t)block[i*4]<<24 | block[i*4+1]<<16 | block[i*4+2]<<8 | block[i*4+3]; \
        for (int i=16;i<64;i++) { \
            uint32_t s0 = rotl32(w[i-15],25)^rotl32(w[i-15],14)^(w[i-15]>>3); \
            uint32_t s1 = rotl32(w[i-2],15)^rotl32(w[i-2],13)^(w[i-2]>>10); \
            w[i] = w[i-16] + s0 + w[i-7] + s1; \
        } \
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7]; \
        for (int i=0;i<64;i++) { \
            uint32_t S1 = rotl32(e,26)^rotl32(e,21)^rotl32(e,7); \
            uint32_t ch = (e&f)^(~e&g); \
            uint32_t t1 = hh + S1 + ch + k[i] + w[i]; \
            uint32_t S0 = rotl32(a,30)^rotl32(a,19)^rotl32(a,10); \
            uint32_t maj = (a&b)^(a&c)^(b&c); \
            uint32_t t2 = S0 + maj; \
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2; \
        } \
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh; \
    }

    /* Main loop */
    while (len--) {
        block[offset++] = *data++;
        if (offset == 64) {
            PROCESS_BLOCK();
            offset = 0;
        }
    }

    /* Padding */
    block[offset++] = 0x80;
    if (offset > 56) {
        memset(block + offset, 0, 64 - offset);
        PROCESS_BLOCK();
        offset = 0;
    }
    memset(block + offset, 0, 56 - offset);
    for (int i = 0; i < 8; ++i)
        block[56 + i] = (uint8_t)(bitlen >> (56 - i*8));
    PROCESS_BLOCK();

    /* Output */
    for (int i = 0; i < 8; ++i) {
        out[i*4]   = h[i] >> 24;
        out[i*4+1] = h[i] >> 16;
        out[i*4+2] = h[i] >> 8;
        out[i*4+3] = h[i];
    }
}

void
hmac_sha1(const uint8_t* key, size_t klen, const uint8_t* msg, size_t mlen, uint8_t out[20])
{
    uint8_t k[64] = {0};
    if (klen > 64) { sha1(key, klen, k); klen = 20; } else memcpy(k, key, klen);
    uint8_t ipad[64], opad[64];
    for (int i=0;i<64;i++) { ipad[i]=k[i]^0x36; opad[i]=k[i]^0x5c; }
    uint8_t tmp[20];
    uint8_t buf[128];
    memcpy(buf, ipad, 64); memcpy(buf+64, msg, mlen);
    sha1(buf, 64+mlen, tmp);
    memcpy(buf, opad, 64); memcpy(buf+64, tmp, 20);
    sha1(buf, 84, out);
}

void
hmac_sha256(const uint8_t* key, size_t klen, const uint8_t* msg, size_t mlen, uint8_t out[32])
{
    uint8_t k[64] = {0};
    if (klen > 64) { sha256(key, klen, k); klen = 32; } else memcpy(k, key, klen);
    uint8_t ipad[64], opad[64];
    for (int i=0;i<64;i++) { ipad[i]=k[i]^0x36; opad[i]=k[i]^0x5c; }
    uint8_t tmp[32];
    uint8_t buf[128];
    memcpy(buf, ipad, 64); memcpy(buf+64, msg, mlen);
    sha256(buf, 64+mlen, tmp);
    memcpy(buf, opad, 64); memcpy(buf+64, tmp, 32);
    sha256(buf, 96, out);
}

int
base32_decode_relaxed(const char* in, uint8_t* out, size_t out_max)
{
    if (!in || !out) return -1;

    uint32_t buffer = 0;
    int bits = 0;
    size_t len = 0;

    while (*in) {
        unsigned char c = *in++;
        uint8_t v;

        if (c >= 'A' && c <= 'Z')      v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a';
        else if (c >= '2' && c <= '7') v = c - '2' + 26;
        else if (c == '=' || c == '-' || c == '_') return -1;  // Reject padding & URL-safe
        else if (c <= ' ') continue;                          // Skip whitespace
        else return -1;                                       // Invalid char

        buffer = (buffer << 5) | v;
        bits += 5;

        if (bits >= 8) {
            if (len >= out_max) return -1;
            out[len++] = (buffer >> (bits - 8)) & 0xFF;
            bits -= 8;
        }
    }

    // Final rule used by ALL real TOTP apps:
    // If there are leftover bits (< 8), just drop them - do NOT reject
    return (int)len;
}

[[maybe_unused]]
int
base32_decode_strict(const char* in, uint8_t* out)
{
    if (!in || !out) return -1;

    uint32_t buf = 0;
    int bits = 0, len = 0;
    while (*in) {
        char c = *in++;
        int v = -1;
        if (c>='A' && c<='Z') v = c-'A';
        else if (c>='a' && c<='z') v = c-'a';
        else if (c>='2' && c<='7') v = c-'2'+26;
        else if (c=='=' || c=='-' || c=='_') {
            return -1; // Padding or URL-safe not allowed in standard TOTP
        } else if (c<=' ') continue; // Ignore whitespace
        else {
            return -1; // Invalid character
        }

        buf = (buf << 5) | v;
        bits += 5;
        if (bits >= 8) {
            out[len++] = (buf >> (bits-8)) & 0xFF;
            bits -= 8;
        }
    }

    if (bits != 0) {
        return -1;
    }
    return len;
}

Error
totp(const char* b32_secret, int digits, uint64_t unix_time, int period, HashAlgo algo, std::string* out_code)
{
    RAISE_CHECK(out_code);
    RAISE_CHECK(digits > 0 && digits <= 10);
    RAISE_CHECK(period > 0);

    uint8_t key[64]; // Max reasonable key size (512 bits)
    int key_len = base32_decode_relaxed(b32_secret, key, sizeof(key));
    if (key_len <= 0) {
        RAISE(ErrorCode::argument, "totp: invalid base32 secret");
    }

    if (key_len > 64) {
        RAISE(ErrorCode::argument, "totp: decoded key length too long");
    }

    uint64_t counter = unix_time / period;
    uint8_t msg[8];
    for (int i=7; i>=0; i--) { msg[i] = counter; counter >>= 8; }

    uint8_t hash[32];
    int offset{};

    switch (algo) {
        case HashAlgo::sha1:
            hmac_sha1(key, key_len, msg, 8, hash);
            offset = hash[19] & 0x0f;
            break;
        case HashAlgo::sha256:
            hmac_sha256(key, key_len, msg, 8, hash);
            offset = hash[31] & 0x0f;
            break;
        default:
            RAISE(ErrorCode::argument, "totp: unsupported hash algorithm: %d", static_cast<int>(algo));
    }

    uint32_t code = ((hash[offset]   & 0x7f) << 24) |
                    ((hash[offset+1] & 0xff) << 16) |
                    ((hash[offset+2] & 0xff) <<  8) |
                     (hash[offset+3] & 0xff);

    uint32_t mod = 1;
    for (int i=0; i<digits; i++) mod *= 10;
    uint32_t out_num = code % mod;
    *out_code = std::to_string(out_num);
    // pad left with zeros if needed
    *out_code = std::string(digits - out_code->length(), '0') + *out_code;
    SUCCEED();
}

class MD5 {
public:
    MD5() { init(); }

    void
    update(const uint8_t* input, size_t length)
    {
        size_t i = 0;

        if (buffer_len > 0) {
            while (i < length && buffer_len < 64) {
                buffer[buffer_len++] = input[i++];
            }
            if (buffer_len == 64) {
                transform(buffer);
                total_len += 512;
                buffer_len = 0;
            }
        }

        while (i + 63 < length) {
            transform(input + i);
            total_len += 512;
            i += 64;
        }

        while (i < length) {
            buffer[buffer_len++] = input[i++];
        }
    }

    void
    update(const std::string& str)
    {
        update(reinterpret_cast<const uint8_t*>(str.c_str()), str.size());
    }

    std::string
    final()
    {
        uint8_t padding[64] = {0x80};
        uint8_t length_bytes[8];

        uint64_t bits = total_len + buffer_len * 8;

        for (int i = 0; i < 8; i++)
            length_bytes[i] = (bits >> (8 * i)) & 0xFF;

        size_t pad_len = (buffer_len < 56) ? (56 - buffer_len) : (120 - buffer_len);
        update(padding, pad_len);
        update(length_bytes, 8);

        std::ostringstream result;
        for (int i = 0; i < 4; i++) {
            result << std::hex << std::setw(2) << std::setfill('0') << (state[i] & 0xFF)
                   << std::setw(2) << ((state[i] >> 8) & 0xFF)
                   << std::setw(2) << ((state[i] >> 16) & 0xFF)
                   << std::setw(2) << ((state[i] >> 24) & 0xFF);
        }

        return result.str();
    }

private:
    uint32_t state[4]{};
    uint64_t total_len = 0;
    uint8_t buffer[64]{};
    size_t buffer_len = 0;

    void
    init()
    {
        state[0] = 0x67452301;
        state[1] = 0xefcdab89;
        state[2] = 0x98badcfe;
        state[3] = 0x10325476;
    }

    static uint32_t
    left_rotate(uint32_t x, uint32_t c)
    {
        return (x << c) | (x >> (32 - c));
    }

    void
    transform(const uint8_t block[64])
    {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], f, g;
        uint32_t m[16];

        for (int i = 0; i < 16; i++) {
            m[i] = (block[i * 4]) |
                   (block[i * 4 + 1] << 8) |
                   (block[i * 4 + 2] << 16) |
                   (block[i * 4 + 3] << 24);
        }

        static const uint32_t k[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
            0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
            0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
            0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
            0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
            0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
            0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
            0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
            0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
        };

        static const uint32_t s[] = {
            7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
            5,9,14,20, 5,9,14,20, 5,9,14,20, 5,9,14,20,
            4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
            6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
        };

        for (int i = 0; i < 64; i++) {
            if (i < 16) {
                f = (b & c) | (~b & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | ~d);
                g = (7 * i) % 16;
            }

            uint32_t temp = d;
            d = c;
            c = b;
            b = b + left_rotate(a + f + k[i] + m[g], s[i]);
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }
};

std::string
md5_str(const std::string& input)
{
    MD5 ctx;
    ctx.update(input);
    return ctx.final();
}

std::string
sha256_str(const std::string& input)
{
    uint8_t hash[32];
    sha256(reinterpret_cast<const uint8_t*>(input.data()), input.size(), hash);

    char hash_hex[65];
    for (size_t i = 0; i < 32; ++i) {
        sprintf(&hash_hex[i * 2], "%02x", hash[i]);
    }
    hash_hex[64] = '\0';
    return std::string(hash_hex);
}

} // namespace orion::crypto