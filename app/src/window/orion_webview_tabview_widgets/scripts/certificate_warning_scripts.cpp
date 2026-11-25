#include "certificate_warning_scripts.h"

namespace orion::gtk {

std::string get_certificate_warning_script()
{
    return R"(
(function() {
    window.CertificateWarningHandler = {
        showCertificate: function() {
            window.webkit.messageHandlers.certificateWarning.postMessage(
                "showCertificate"
            );
        },

        skipCertificateWarning: function() {
            window.webkit.messageHandlers.certificateWarning.postMessage(
                "skipCertificateWarning"
            );
        }
    };
})();
)";
}

} // namespace orion::gtk