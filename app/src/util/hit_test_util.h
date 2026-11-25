#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <vector>
#include <webkit/webkit.h>

class HitTestUtil
{
public:
    enum class Element{
        editable,
        image,
        link,
        media,
        scrollbar,
        selection,
        all
    };

    static std::vector<Element> get_elements_from_hit_test(WebKitHitTestResult* result);
};
#endif