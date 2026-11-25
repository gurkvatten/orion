#include <vector>
#include <cstdint>

#include <src/punycode.h>

namespace orion::punycode {

// Punycode constants from RFC 3492
constexpr int32_t base = 36;
constexpr int32_t tmin = 1;
constexpr int32_t tmax = 26;
constexpr int32_t skew = 38;
constexpr int32_t damp = 700;
constexpr int32_t initial_bias = 72;
constexpr int32_t initial_n = 128;
constexpr char delimiter = '-';

// Helper functions
static int32_t
adapt(int32_t delta, int32_t num_points, bool first_time)
{
    delta = first_time ? delta / damp : delta / 2;
    delta += delta / num_points;
    int32_t k = 0;
    while (delta > ((base - tmin) * tmax) / 2) {
        delta /= base - tmin;
        k += base;
    }
    return k + (base - tmin + 1) * delta / (delta + skew);
}

// Encode a single code point to a basic code (ASCII character)
static char
encode_digit(int32_t d)
{
    return d + 22 + 75 * (d < 26); // 0..25 -> 'a'..'z', 26..35 -> '0'..'9'
}

// Main Punycode encoder (for a single label, e.g., "täst")
static std::string
encode_label(const std::string& input)
{
    std::vector<uint32_t> input_codepoints;
    
    // Convert UTF-8 string to Unicode code points
    for (size_t i = 0; i < input.size();) {
        unsigned char c = input[i];
        uint32_t cp = 0;
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = ((c & 0x1F) << 6) | (input[i + 1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = ((c & 0x0F) << 12) | ((input[i + 1] & 0x3F) << 6) | (input[i + 2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = ((c & 0x07) << 18) | ((input[i + 1] & 0x3F) << 12) | ((input[i + 2] & 0x3F) << 6) | (input[i + 3] & 0x3F);
            i += 4;
        }
        input_codepoints.push_back(cp);
    }

    // Separate basic (ASCII) characters
    std::string output;
    for (uint32_t cp : input_codepoints) {
        if (cp < 0x80) output += static_cast<char>(cp);
    }
    size_t b = output.size();
    size_t h = b;

    if (b > 0) output += delimiter;

    uint32_t n = initial_n;
    int32_t delta = 0;
    int32_t bias = initial_bias;

    while (h < input_codepoints.size()) {
        uint32_t m = UINT32_MAX;
        for (auto cp : input_codepoints) if (cp >= n && cp < m) m = cp;

        delta += (m - n) * (h + 1);
        n = m;

        for (size_t j = 0; j < input_codepoints.size(); ++j) {
            uint32_t cp = input_codepoints[j];
            if (cp < n) {
                delta++;
            } else if (cp == n) {
                int32_t q = delta;
                for (int32_t k = base;; k += base) {
                    int32_t t = std::max(tmin, std::min(tmax, k - bias));
                    if (q < t) break;
                    output += encode_digit(t + (q - t) % (base - t));
                    q = (q - t) / (base - t);
                }
                output += encode_digit(q);
                bias = adapt(delta, h + 1, h == b);
                delta = 0;
                h++;
            }
        }
        delta++;
        n++;
    }

    return output;
}

// Encode full domain (splitting by '.')
std::string
encode(const std::string& unicode_domain)
{
    std::string result;
    size_t start = 0, end = 0;
    while (end <= unicode_domain.size()) {
        if (end == unicode_domain.size() || unicode_domain[end] == '.') {
            std::string label = unicode_domain.substr(start, end - start);
            bool needs_punycode = false;
            for (char c : label) {
                if (static_cast<unsigned char>(c) >= 128) {
                    needs_punycode = true;
                    break;
                }
            }

            if (needs_punycode) {
                result += "xn--" + encode_label(label);
            } else {
                result += label;
            }

            if (end != unicode_domain.size()) result += '.';
            start = end + 1;
        }
        end++;
    }
    return result;
}

static int32_t
decode_digit(char cp)
{
    if (cp >= '0' && cp <= '9') return cp - '0' + 26;
    if (cp >= 'a' && cp <= 'z') return cp - 'a';
    if (cp >= 'A' && cp <= 'Z') return cp - 'A';
    return -1;
}

static std::string
utf8_encode(uint32_t cp)
{
    std::string out;

    if (cp <= 0x7F) {
        out.push_back(cp);
    } else if (cp <= 0x7FF) {
        out.push_back(0xC0 | (cp >> 6));
        out.push_back(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out.push_back(0xE0 | (cp >> 12));
        out.push_back(0x80 | ((cp >> 6) & 0x3F));
        out.push_back(0x80 | (cp & 0x3F));
    } else {
        out.push_back(0xF0 | (cp >> 18));
        out.push_back(0x80 | ((cp >> 12) & 0x3F));
        out.push_back(0x80 | ((cp >> 6) & 0x3F));
        out.push_back(0x80 | (cp & 0x3F));
    }

    return out;
}

static std::string
decode_label(const std::string& input)
{
    int32_t n = initial_n;
    int32_t i = 0;
    int32_t bias = initial_bias;

    std::vector<uint32_t> output;

    size_t d = input.find_last_of(delimiter);
    if (d != std::string::npos) {
        for (size_t j = 0; j < d; ++j)
            output.push_back(static_cast<uint32_t>(input[j]));
        d++;
    } else {
        d = 0;
    }

    size_t in = d;

    while (in < input.size()) {

        int32_t oldi = i;
        int32_t w = 1;

        for (int32_t k = base;; k += base) {

            if (in >= input.size())
                return "";

            int32_t digit = decode_digit(input[in++]);
            if (digit < 0)
                return "";

            i += digit * w;

            int32_t t{};
            if (k <= bias) {
                t = tmin;
            } else if (k >= bias + tmax) {
                t = tmax;
            } else {
                t = k - bias;
            }

            if (digit < t)
                break;

            w *= (base - t);
        }

        bias = adapt(i - oldi, output.size() + 1, oldi == 0);
        n += i / (output.size() + 1);
        i = i % (output.size() + 1);

        output.insert(output.begin() + i, n);
        i++;
    }

    std::string result;
    for (auto cp : output) {
        result += utf8_encode(cp);
    }

    return result;
}

std::string
decode(const std::string& domain)
{
    std::string result;
    size_t start = 0;
    size_t end = 0;

    while (end <= domain.size()) {
        if (end == domain.size() || domain[end] == '.') {
            std::string label = domain.substr(start, end - start);

            if (label.rfind("xn--", 0) == 0) {
                result += decode_label(label.substr(4));
            } else {
                result += label;
            }

            if (end != domain.size()) {
                result += '.';
            }

            start = end + 1;
        }

        end++;
    }

    return result;
}

} // namespace orion::punycode