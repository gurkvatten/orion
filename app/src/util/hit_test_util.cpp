
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include "hit_test_util.h"
#include <common/src/log.h>
#include <vector>

std::vector<HitTestUtil::Element>
HitTestUtil::get_elements_from_hit_test(WebKitHitTestResult* result)
{
    std::vector<Element> elements;

    if (webkit_hit_test_result_context_is_editable(result)) {
        //orion::log_debug("Editable");
        elements.push_back(Element::editable);
    }
    if (webkit_hit_test_result_context_is_image(result)) {
        //orion::log_debug("Image");
        elements.push_back(Element::image);
    }
    if (webkit_hit_test_result_context_is_link(result)) {
        //orion::log_debug("Link");
        elements.push_back(Element::link);
    }
    if (webkit_hit_test_result_context_is_media(result)) {
        //orion::log_debug("Media");
        elements.push_back(Element::media);
    }
    if (webkit_hit_test_result_context_is_scrollbar(result)) {
        //orion::log_debug("Scrollbar");
        elements.push_back(Element::scrollbar);
    }
    if (webkit_hit_test_result_context_is_selection(result)) {
        //orion::log_debug("Selection");
        elements.push_back(Element::selection);
    }

    if (elements.empty()){
        //orion::log_debug("All");
        elements.push_back(Element::all);
    }
    
    return elements;
}

#endif