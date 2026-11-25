#include <core/scripts/allow_copy_and_paste_scripts.h>

namespace orion::scripts {

const std::string
get_allow_copy_and_paste_script()
{
    return R"(
(() => {
    const original = Event.prototype.preventDefault;

    const unblock = new Set([
        "copy",
        "cut",
        "paste"
    ]);

    Event.prototype.preventDefault = function() {
        if (unblock.has(this.type))
            return;

        return original.call(this);
    };
})();
)";
}

} // namespace orion::scripts