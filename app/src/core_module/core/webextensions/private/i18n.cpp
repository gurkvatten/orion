#include <core/webextensions/extension.h>
#include <core/webextensions/private/i18n.h>

#include <rfl/json.hpp>

#include <algorithm>
#include <sstream>

namespace WebExtensions {

std::string
I18nImpl::script_source_for_extension(WebExtension* extension)
{
    const std::string default_locale = extension->default_locale().value_or("en_US");
    std::string ui_language = default_locale;
    std::replace(ui_language.begin(), ui_language.end(), '_', '-');

    std::string messages = "{}";
    const std::string messages_path = "_locales/" + default_locale + "/messages.json";
    if(const auto messages_data = extension->resources().data_for_path(messages_path)) {
        messages.assign(
            reinterpret_cast<const char*>(messages_data->data),
            messages_data->datalen);
    }

    const std::string extension_id = extension->extension_id().value;
    const std::string language = ui_language.substr(0, ui_language.find('-'));
    const bool right_to_left =
        language == "ar" || language == "fa" || language == "he"
        || language == "ps" || language == "ur";

    std::stringstream script;
    script << R"v2i18n(
(function(){
    const implementations = window.__webextensions_v2_implementations__ =
        window.__webextensions_v2_implementations__ || {};
    const uiLanguage = )v2i18n" << rfl::json::write(ui_language) << R"v2i18n(;
    const uiLocale = )v2i18n" << rfl::json::write(default_locale) << R"v2i18n(;
    const extensionId = )v2i18n" << rfl::json::write(extension_id) << R"v2i18n(;
    const rightToLeft = )v2i18n" << (right_to_left ? "true" : "false") << R"v2i18n(;
    const localeMessages = )v2i18n" << messages << R"v2i18n(;
    const messages = Object.create(null);

    for(const [name, record] of Object.entries(localeMessages)) {
        messages[name.toLowerCase()] = record;
    }

    function predefinedMessage(name) {
        switch(name.toLowerCase()) {
        case "@@extension_id": return extensionId;
        case "@@ui_locale": return uiLocale;
        case "@@bidi_dir": return rightToLeft ? "rtl" : "ltr";
        case "@@bidi_reversed_dir": return rightToLeft ? "ltr" : "rtl";
        case "@@bidi_start_edge": return rightToLeft ? "right" : "left";
        case "@@bidi_end_edge": return rightToLeft ? "left" : "right";
        default: return undefined;
        }
    }

    function normalizeSubstitutions(value) {
        if(value === undefined) return [];
        const substitutions = Array.isArray(value) ? value : [value];
        if(substitutions.length > 9) return undefined;
        return substitutions.map((substitution) => String(substitution));
    }

    function expandMessage(record, substitutions) {
        let message = typeof record.message === "string" ? record.message : "";
        const placeholders = Object.create(null);

        if(record.placeholders && typeof record.placeholders === "object") {
            for(const [name, placeholder] of Object.entries(record.placeholders)) {
                if(placeholder && typeof placeholder.content === "string") {
                    placeholders[name.toLowerCase()] = placeholder.content;
                }
            }
        }

        message = message.replace(/\$([A-Za-z0-9_]+)\$/g, (match, name) =>
            Object.prototype.hasOwnProperty.call(placeholders, name.toLowerCase())
                ? placeholders[name.toLowerCase()]
                : match);
        message = message.replace(/\$([1-9])/g, (match, index) =>
            substitutions[Number(index) - 1] === undefined
                ? ""
                : substitutions[Number(index) - 1]);
        return message.replace(/\$\$/g, "$");
    }

    implementations["I18n.getMessage"] = (context) => {
        const messageName = context.args.messageName;
        const predefined = predefinedMessage(messageName);
        if(predefined !== undefined) return predefined;

        const substitutions = normalizeSubstitutions(context.args.substitutions);
        if(substitutions === undefined) return undefined;

        const record = messages[messageName.toLowerCase()];
        if(!record || typeof record !== "object") return "";

        let result = expandMessage(record, substitutions);
        if(context.args.options && context.args.options.escapeLt) {
            result = result.replace(/</g, "&lt;");
        }
        return result;
    };

    implementations["I18n.getUILanguage"] = () => uiLanguage;
})();
)v2i18n";
    return script.str();
}

void
I18nImpl::detectLanguage(
    v2::Promise<v2::I18nLanguageDetectionResult> promise,
    const std::string&)
{
    const v2::I18nLanguageDetectionResultStorage result {
        .isReliable = false,
        .languages = {{v2::I18nDetectedLanguageStorage {
            .language = "en-US",
            .percentage = 100,
        }}},
    };
    promise.resolve(v2::from_storage(result));
}

void
I18nImpl::getAcceptLanguages(v2::Promise<std::vector<std::string>> promise)
{
    promise.resolve(std::vector<std::string> {"en-US"});
}

void
I18nImpl::getPreferredSystemLanguages(v2::Promise<std::vector<std::string>> promise)
{
    promise.resolve(std::vector<std::string> {"en-US"});
}

} // namespace WebExtensions
