#define ENABLE_PUNYCODE_TEST !NDEBUG && 1
#define ENABLE_VERBOSE_LOGS 0

#include <string>

#include <src/punycode_test.h>
#include <src/punycode.h>
#include <src/error.h>
#include <src/log.h>

namespace orion::punycode {

namespace {
    struct Case {
        std::string input;
        std::string expected;
    };
}

[[maybe_unused]]
static Error
test()
{
    Case cases[] = {
        {"hello", "hello"},
        {"café", "xn--caf-dma"},
        {"täst.com", "xn--tst-qla.com"},
        {"école.fr", "xn--cole-9oa.fr"},
        {"bücher.de", "xn--bcher-kva.de"},
        {"niño.ws", "xn--nio-8ma.ws"},
        {"العربية.ws", "xn--mgbcd4a2b0d2b.ws"},
        {"test中文", "xn--test-3f5fy05j"},
        {"测试", "xn--0zwm56d"},
        {"münchen.bayern", "xn--mnchen-3ya.bayern"},
        {"čáslav.cz", "xn--slav-4na7x.cz"},
        {"žluté.lázně.cz", "xn--lut-dma55e.xn--lzn-ela60a.cz"},
        {"παράδειγμα.δοκιμή", "xn--hxajbheg2az3al.xn--jxalpdlp"},
        {"例子.测试", "xn--fsqu00a.xn--0zwm56d"},
        {"उदाहरण.परीक्षा", "xn--p1b6ci4b4b3a.xn--11b5bs3a9aj6g"},
        {"مثال.إختبار", "xn--mgbh0fb.xn--kgbechtv"},
        {"пример.испытание", "xn--e1afmkfd.xn--80akhbyknj4f"},
    };

    for (const auto& c : cases) {
        std::string output = encode(c.input);
        if (output != c.expected) {
            RAISE(ErrorCode::assert, "punycode_test: test failed for input '%s': expected '%s', got '%s'",
                c.input.c_str(),
                c.expected.c_str(),
                output.c_str()
            );
        }

        std::string decoded = decode(encode(c.input));
        if (decoded != c.input) {
            RAISE(ErrorCode::assert, "punycode_test: roundtrip test failed for input '%s': expected '%s', got '%s'",
                c.input.c_str(),
                c.input.c_str(),
                decoded.c_str()
            );
        }

        log_verbose("punycode_test: test passed for input '%s'", c.input.c_str());
    }
    SUCCEED();
}

void
punycode_test()
{
#if ENABLE_PUNYCODE_TEST
    VERIFY_LOG(test());
#endif
}

} // orion::punycode