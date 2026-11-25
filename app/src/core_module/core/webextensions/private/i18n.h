#pragma once

#include <core/core_api.h>

namespace WebExtensions {

class I18nImpl : public v2::I18n {
public:
    static std::string script_source_for_extension(WebExtension* extension);
    void detectLanguage(
        v2::Promise<v2::I18nLanguageDetectionResult> promise,
        const std::string& text) override;
    void getAcceptLanguages(v2::Promise<std::vector<std::string>> promise) override;
    void getPreferredSystemLanguages(v2::Promise<std::vector<std::string>> promise) override;
};

} // namespace WebExtensions
