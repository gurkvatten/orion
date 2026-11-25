#define PRINT_JSON_FILTERS 0

#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_set>

#include <uv.h>

#include "filter_list.h"
#include <src/string_util.h>
#include <src/punycode.h>

#if PRINT_JSON_FILTERS
#   include <rfl/json.hpp>
#endif

// Adapted from Mac Orion version, see also: Kagi/Core/Utilities/EasyListConverter.swift

namespace orion {

#define RESOURCE_TYPES(X) \
    X(other,            0x00000001) \
    X(xbl,              0x00000001) \
    X(dtd,              0x00000001) \
    X(script,           0x00000002) \
    X(image,            0x00000004) \
    X(background,       0x00000004) \
    X(stylesheet,       0x00000008) \
    X(object,           0x00000010) \
    X(subdocument,      0x00000020) \
    X(document,         0x00000040) \
    X(ping,             0x00000100) \
    X(xmlhttprequest,   0x00000800) \
    X(object_subrequest,0x00001000) \
    X(media,            0x00004000) \
    X(font,             0x00008000) \
    X(popup,            0x10000000) \
    X(websocket,        0x20000000) \
    X(elemhide,         0x40000000) \
    X(redirect,         0x80000000) \
    X(all,              0x7fffffff)

enum ResourceType : uint32_t {
#define ENTRY(name, value) name = value,
    RESOURCE_TYPES(ENTRY)
#undef ENTRY
};

const std::unordered_map<ResourceType, std::string> global_resource_type_value_to_name = {
#define ENTRY(name, val) {ResourceType::name, #name},
    RESOURCE_TYPES(ENTRY)
#undef ENTRY
};

const std::unordered_map<std::string, ResourceType> global_resource_type_name_to_value = {
#define ENTRY(name, val) {#name, ResourceType::name},
    RESOURCE_TYPES(ENTRY)
#undef ENTRY
};

#undef RESOURCE_TYPES

namespace { // anonymous namespace to force static storage

struct CosmeticFilterInfo {
    std::string selector{};
    bool is_generic = false;
    std::vector<std::string> domains{};
    bool is_exception = false;
    bool uses_unless_domain = false;
};

struct CreateFilterQuery {
    std::string source{};
    std::optional<uint32_t> content_type{};
    std::optional<bool> match_case{};
    std::optional<std::string> domains{};
    std::optional<std::string> frame_urls{};
    std::optional<bool> third_party{};
    std::optional<std::string> sitekeys{};
};

} // anonymous namespace

struct FilterList {
    uint32_t resource_type_prototype = ResourceType::all;
    bool is_easy_list = false;

    std::regex cosmetic_filter_regex{};
    std::regex quantifier_regex{};
    std::regex elemhide_regex{};
    std::regex group_regex{};

    std::vector<std::string> generic_cosmetic_filters{};
    std::vector<CosmeticFilterInfo> cosmetic_filters_with_if_top_domains_exceptions{};
    std::vector<CosmeticFilterInfo> cosmetic_filters_with_if_top_domains{};
    std::unordered_map<std::string, std::set<std::string>> generic_cosmetic_exceptions{};

    std::vector<std::string> generic_hide_whitelist_filters{};
    std::vector<std::string> generic_block_whitelist_filters{};
    std::unordered_set<std::string> badfilter_patterns{};
    std::optional<bool> is_low_end_device{};

    static const size_t max_supported_rule_count = 150000;
    static const size_t max_selectors_per_rule = 250;
};

static FilterListItem
cosmetic_filter_info_to_rule(const CosmeticFilterInfo& info)
{
    FilterListItem item{};

    item["trigger"]["url-filter"] = ".*";
    item["action"]["type"] = "css-display-none";
    item["action"]["selector"] = info.selector;

    if (!info.domains.empty()) {
        item["trigger"][info.uses_unless_domain ? "unless-domain" : "if-domain"] = info.domains;
    }

    return item;
}

static std::vector<FilterListItem>
optimized_rules(const std::vector<CosmeticFilterInfo>& infos, bool is_exception, size_t max_selectors_per_rule)
{
    std::vector<FilterListItem> rules{};
    struct Group {
        std::vector<std::string> domains{};
        std::vector<std::string> selectors{};
    };
    std::unordered_map<std::string, Group> groups{};
    std::vector<std::string> group_order{};

    for (const CosmeticFilterInfo& info : infos) {
        if (info.uses_unless_domain || info.domains.empty()) {
            rules.push_back(cosmetic_filter_info_to_rule(info));
            continue;
        }

        std::vector<std::string> domains = info.domains;
        std::sort(domains.begin(), domains.end());
        domains.erase(std::unique(domains.begin(), domains.end()), domains.end());
        const std::string key = orion::str::join_strings(domains, "\x1f");
        auto it = groups.find(key);
        if (it != groups.end()) {
            it->second.selectors.push_back(info.selector);
        } else {
            groups.emplace(key, Group{.domains = std::move(domains), .selectors = {info.selector}});
            group_order.push_back(key);
        }
    }

    for (const std::string& key : group_order) {
        Group& group = groups.at(key);
        for (size_t offset = 0; offset < group.selectors.size(); offset += max_selectors_per_rule) {
            // Use paranthesized (std::min) to avoid breaking Windows compilation
            const size_t end = (std::min)(offset + max_selectors_per_rule, group.selectors.size());
            std::vector<std::string> selectors(group.selectors.begin() + offset, group.selectors.begin() + end);
            FilterListItem item{};
            item["trigger"] = {
                {"url-filter", ".*"},
                {is_exception ? "unless-domain" : "if-domain", group.domains}
            };
            item["action"] = {
                {"type", "css-display-none"},
                {"selector", orion::str::join_strings(selectors, ", ")}
            };
            rules.push_back(item);
        }
    }
    return rules;
}

static std::string
replace_all(std::string value, const std::string& needle, const std::string& replacement)
{
    size_t position = 0;
    while ((position = value.find(needle, position)) != std::string::npos) {
        value.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
    return value;
}

static std::string
apply_combined_cleaner(const std::string& source, bool domain_only)
{
    std::string result{};
    result.reserve(source.size() + 16);

    for (size_t i = 0; i < source.size();) {
        const char c = source[i];
        if (c == '\\' && i + 1 < source.size()) {
            const char shorthand = source[i + 1];
            const char* replacement = nullptr;
            switch (shorthand) {
            case 'd': replacement = "[0-9]"; break;
            case 'D': replacement = "[^0-9]"; break;
            case 'w': replacement = "[a-zA-Z0-9_]"; break;
            case 'W': replacement = "[^a-zA-Z0-9_]"; break;
            case 's': if (!domain_only) replacement = "[\\r\\n\\t\\f\\v ]"; break;
            case 'S': if (!domain_only) replacement = "[^\\r\\n\\t\\f\\v ]"; break;
            }
            if (replacement) {
                result += replacement;
                i += 2;
                continue;
            }
        }

        if (domain_only) {
            result += c;
        } else if (c == '*') {
            result += ".*";
        } else if (c == '^') {
            result += "[:/]";
        } else if (c == '|') {
            if (i == 0) {
                result += '^';
            } else if (i + 1 == source.size()) {
                result += '$';
            } else {
                result += "\\|";
            }
        } else {
            static constexpr std::string_view meta_characters = ".+?$(){}[]\\";
            if (meta_characters.find(c) != std::string_view::npos) {
                result += '\\';
            }
            result += c;
        }
        ++i;
    }
    return result;
}

static std::string
apply_regex_shorthand_cleaner(const std::string& source)
{
    std::string result{};
    result.reserve(source.size() + 16);
    for (size_t i = 0; i < source.size();) {
        if (source[i] == '\\' && i + 1 < source.size()) {
            const char* replacement = nullptr;
            switch (source[i + 1]) {
            case 'd': replacement = "[0-9]"; break;
            case 'D': replacement = "[^0-9]"; break;
            case 'w': replacement = "[a-zA-Z0-9_]"; break;
            case 'W': replacement = "[^a-zA-Z0-9_]"; break;
            case 's': replacement = "[\\r\\n\\t\\f\\v ]"; break;
            case 'S': replacement = "[^\\r\\n\\t\\f\\v ]"; break;
            }
            if (replacement) {
                result += replacement;
                i += 2;
                continue;
            }
        }
        result += source[i++];
    }
    return result;
}

// Returns regex
static std::string
filter_list_process_source(const FilterList* self, const std::string& source, bool domain_only = false)
{
    bool anchor = false;
    std::string regex{};

    if (source.length() >= 2 && source.starts_with("/") && source.ends_with("/")) {
        // Keep the closing slash so a regex ending in `$/` can be converted below.
        regex = source.substr(1);
        regex = std::regex_replace(regex, std::regex(R"(\(\\d\)\{1,3\})"), "[0-9]?[0-9]?[0-9]");
        regex = apply_regex_shorthand_cleaner(regex);
    } else {
        regex = source;
        if (regex.starts_with("||")) {
            regex = regex.substr(2);
            anchor = true;
        }

        std::string collapsed{};
        collapsed.reserve(regex.size());
        for (char c : regex) {
            if (c != '*' || collapsed.empty() || collapsed.back() != '*') {
                collapsed += c;
            }
        }
        regex = std::move(collapsed);
        if (regex.ends_with("^|")) {
            regex.resize(regex.size() - 2);
        }
        regex = replace_all(regex, R"((\d){1,3})", "[0-9]?[0-9]?[0-9]");
        regex = apply_combined_cleaner(regex, domain_only);
    }

    if (!regex.starts_with("://") && !regex.starts_with("^")) {
        if (anchor) {
            regex = R"(^[^:]+://+([^:/]+\.)?)" + regex;
        }
    } else if (regex.starts_with("^") && regex.find("://") == std::string::npos
        && !regex.starts_with("^https") && regex.find(R"(:\/\/)") == std::string::npos) {
        regex = R"(^[^:]+://+)" + regex.substr(1);
    }

    if (regex.ends_with("$/")) {
        regex = replace_all(regex, "$/", "([/:?#].*)?$");
    }

    if (regex.empty()) {
        regex = ".*";
    }
    return regex;
}

static bool
filter_list_has_unsupported_regex_disjunction(const std::string& source)
{
    if (source.size() < 2 || !source.starts_with("/") || !source.ends_with("/")) {
        return false;
    }

    bool escaped = false;
    bool in_character_class = false;
    for (size_t i = 1; i + 1 < source.size(); ++i) {
        const char c = source[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
        } else if (c == '[' && !in_character_class) {
            in_character_class = true;
        } else if (c == ']' && in_character_class) {
            in_character_class = false;
        } else if (c == '|' && !in_character_class) {
            return true;
        }
    }
    return false;
}

static Error
filter_list_process_domains(
    FilterList* self,
    std::string domain_string,
    std::vector<std::string>* out_included_domains,
    std::vector<std::string>* out_excluded_domains)
{
    RAISE_CHECK(out_included_domains);
    RAISE_CHECK(out_excluded_domains);

    orion::str::to_lower(&domain_string);
    std::deque<std::string> domains = orion::str::split(domain_string, '|');

    for (std::string d : domains) {
        bool is_included = true;
        if (d.starts_with("~")) {
            d = d.substr(1);
            is_included = false;
        }
        
        std::string encoded_domain = d;
        if (!orion::str::is_ascii(d)) {
            encoded_domain = orion::punycode::encode(d);
        }

        if (is_included) {
            out_included_domains->push_back("*" + encoded_domain);
        } else {
            out_excluded_domains->push_back("*" + encoded_domain);
        }
    }

    SUCCEED();
}

static std::vector<std::string>
filter_list_process_frame_urls(const FilterList* self, const std::string& frame_urls_string)
{
    std::vector<std::string> frame_urls{};
    for (const std::string& frame_url : orion::str::split(frame_urls_string, '|')) {
        frame_urls.push_back(filter_list_process_source(self, frame_url));
    }
    return frame_urls;
}

static Error
filter_list_create_filter(FilterList* self, CreateFilterQuery query, FilterListItem* out_item, bool* out_ok)
{
    RAISE_CHECK(out_item);
    RAISE_CHECK(out_ok);

    if (!orion::str::is_ascii(query.source) || query.source.find('$') != std::string::npos) {
        *out_ok = false;
        SUCCEED();
    }
    if (std::regex_search(query.source, self->quantifier_regex)) {
        *out_ok = false;
        SUCCEED();
    }

    std::string regex = filter_list_process_source(self, query.source);

    (*out_item)["trigger"]["url-filter"] = regex;
    (*out_item)["action"]["type"] = "block";

    if (query.third_party.has_value()) {
        (*out_item)["trigger"]["load-type"] = std::vector<std::string>{
            query.third_party.value() ? "third-party" : "first-party"
        };
    }

    if (query.match_case.has_value() && query.match_case.value()) {
        (*out_item)["trigger"]["url-filter-is-case-sensitive"] = true;
    }

    if (query.domains.has_value()) {
        std::vector<std::string> included_domains{};
        std::vector<std::string> excluded_domains{};
        VERIFY(filter_list_process_domains(self, query.domains.value(), &included_domains, &excluded_domains));
        if (included_domains.empty() && excluded_domains.empty()) {
            *out_ok = false;
            SUCCEED();
        }

        if (!included_domains.empty()) {
            (*out_item)["trigger"]["if-domain"] = included_domains;
        } else if (!excluded_domains.empty()) {
            (*out_item)["trigger"]["unless-domain"] = excluded_domains;
        }
    } else if (query.frame_urls.has_value()) {
        std::vector<std::string> frame_urls = filter_list_process_frame_urls(self, query.frame_urls.value());
        if (!frame_urls.empty()) {
            (*out_item)["trigger"]["if-frame-url"] = frame_urls;
        }
    }

    if (query.content_type.has_value()) {
        std::vector<std::string> rt; // resource types
        uint32_t ct = query.content_type.value();
        const bool has_document = ct & ResourceType::document;
        const bool has_popup = ct & ResourceType::popup;
        const bool has_subdocument = ct & ResourceType::subdocument;

        if (has_document || has_popup) {
            rt.push_back("document");
        }
        if (ct & ResourceType::image) {
            rt.push_back("image");
        }
        if (ct & ResourceType::stylesheet) {
            rt.push_back("style-sheet");
        }
        if (ct & ResourceType::script) {
            rt.push_back("script");
        }
        if (ct & ResourceType::font) {
            rt.push_back("font");
        }
        if (ct & ResourceType::xmlhttprequest) {
            rt.push_back("fetch");
        }
        if (ct & ResourceType::media) {
            rt.push_back("media");
        }
        if (ct & ResourceType::ping) {
            rt.push_back("ping");
        }
        if (ct & ResourceType::websocket) {
            rt.push_back("websocket");
        }
        if (ct & ResourceType::other) {
            rt.push_back("other");
        }
        if (ct & ResourceType::redirect) {
            rt.push_back("raw");
        }

        if (!rt.empty()) {
            (*out_item)["trigger"]["resource-type"] = rt;
        }
        if (has_subdocument && !has_document) {
            (*out_item)["trigger"]["load-context"] = std::vector<std::string>{"child-frame"};
        }
    }

    *out_ok = true;
    SUCCEED();
}

static std::string
filter_list_normalize_option(std::string option)
{
    std::replace(option.begin(), option.end(), '-', '_');
    orion::str::to_lower(&option);

    static const std::unordered_map<std::string, std::string> aliases = {
        {"xhr", "xmlhttprequest"},
        {"css", "stylesheet"},
        {"frame", "subdocument"},
        {"doc", "document"},
        {"ehide", "elemhide"},
        {"ghide", "generichide"},
        {"shide", "specifichide"},
    };

    const bool negated = option.starts_with("~");
    const std::string alias_key = negated ? option.substr(1) : option;
    if (auto it = aliases.find(alias_key); it != aliases.end()) {
        return negated ? "~" + it->second : it->second;
    }
    return option;
}

static bool
filter_list_has_valid_options(const std::string& options)
{
    if (options.empty()) {
        return false;
    }

    for (const std::string& option : orion::str::split(options, ',')) {
        if (option.empty()) {
            return false;
        }
        const size_t separator_index = option.find('=');
        const size_t key_length = separator_index == std::string::npos ? option.size() : separator_index;
        const std::string_view key(option.data(), key_length);
        size_t key_offset = key.starts_with("~") ? 1 : 0;
        if (key_offset == key.size()) {
            return false;
        }
        for (; key_offset < key.size(); ++key_offset) {
            const unsigned char c = key[key_offset];
            if (!std::isalnum(c) && c != '_' && c != '-') {
                return false;
            }
        }
        if (separator_index != std::string::npos) {
            if (separator_index + 1 == option.size()) {
                return false;
            }
            for (size_t i = separator_index + 1; i < option.size(); ++i) {
                if (std::isspace(static_cast<unsigned char>(option[i]))) {
                    return false;
                }
            }
        }
    }
    return true;
}

static std::optional<std::string>
filter_list_options_part(const std::string& text, size_t first_dollar_index)
{
    for (size_t dollar_index = first_dollar_index; dollar_index != std::string::npos;
         dollar_index = text.find('$', dollar_index + 1)) {
        std::string options = text.substr(dollar_index + 1);
        if (filter_list_has_valid_options(options)) {
            return options;
        }
    }
    return std::nullopt;
}

static Error
filter_list_parse_filter(
    FilterList* self, std::string text, FilterListItem* out_item, bool* out_ok, bool* out_is_generic)
{
    RAISE_CHECK(out_item);
    RAISE_CHECK(out_ok);
    RAISE_CHECK(out_is_generic);

    *out_ok = false;
    *out_is_generic = false;

    bool is_whitelist = text.starts_with("@@");
    bool blocking = !is_whitelist;
    bool generic_hide = false;
    bool generic_block = false;
    bool specific_hide = false;

    if (is_whitelist) {
        text = text.substr(2);
    }

    size_t dollar_index = text.find('$');
    const std::optional<std::string> options_part = dollar_index == std::string::npos
        ? std::nullopt
        : filter_list_options_part(text, dollar_index);

    // set up values for options if set
    std::optional<uint32_t> content_type{};
    std::optional<bool> match_case{};
    std::optional<std::string> domains{};
    std::optional<std::string> frame_urls{};
    std::optional<std::string> sitekeys{};
    std::optional<bool> third_party{};

    if (dollar_index != std::string::npos && options_part.has_value()) {
        std::deque<std::string> options = orion::str::split(options_part.value(), ',');
        text = text.substr(0, dollar_index);
        
        if (std::regex_search(text, self->quantifier_regex)) {
            SUCCEED();
        }

        if (filter_list_has_unsupported_regex_disjunction(text)) {
            SUCCEED();
        }

        if (std::regex_search(text, self->group_regex)) {
            SUCCEED();
        }

        std::optional<std::string> value;

        for (const std::string& raw_option : options) {
            std::string key;
            size_t separator_index = raw_option.find('=');

            if (separator_index != std::string::npos) {
                key = raw_option.substr(0, separator_index);
                value = raw_option.substr(separator_index + 1);
            } else {
                key = raw_option;
            }

            key = filter_list_normalize_option(std::move(key));

            if (!key.empty() && std::all_of(key.begin(), key.end(), [](char c) { return c == '_'; })) {
                continue;
            }

            const auto& m = global_resource_type_name_to_value;

            if (key == "important" || key == "all") {
                continue;
            } else if (m.find(key) != m.end()) {
                ResourceType type = m.at(key);
                content_type = content_type.value_or(0) | type;
            } else if (key.starts_with("~") && m.find(key.substr(1)) != m.end()) {
                ResourceType type = m.at(key.substr(1));
                content_type = content_type.value_or(self->resource_type_prototype) & ~type;
            } else if (key == "match_case") {
                match_case = true;
            } else if (key == "~match_case") {
                match_case = false;
            } else if (key == "domain" && value.has_value()) {
                domains = value.value();
            } else if (key == "frameurl" && value.has_value()) {
                frame_urls = value.value();
            } else if (key == "third_party" || key == "3p" || key == "~1p") {
                third_party = true;
            } else if (key == "~third_party" || key == "~3p" || key == "1p") {
                third_party = false;
            } else if (key == "sitekey" && value.has_value()) {
                sitekeys = value.value();
            } else if (key == "denyallow") {
                SUCCEED();
            } else if (key == "badfilter") {
                SUCCEED();
            } else if (key == "generichide") {
                generic_hide = true;
            } else if (key == "genericblock") {
                generic_block = true;
            } else if (key == "specifichide") {
                specific_hide = true;
            } else {
                SUCCEED();
            }
        }
    } else {
        if (std::regex_search(text, self->quantifier_regex)) {
            SUCCEED();
        }

        if (filter_list_has_unsupported_regex_disjunction(text)) {
            SUCCEED();
        }
    }

    *out_is_generic = generic_hide || generic_block || specific_hide;

    if (*out_is_generic && text.find('/') == std::string::npos && text.find('*') == std::string::npos) {
        // Remove each occurrence of "||" and "^"
        std::string filter_domain = std::regex_replace(text, std::regex(R"(\|\||\^)"), "");
        std::vector<std::string> domain_vec{};
        if (filter_domain.size() > 2) {
            domain_vec.push_back("*" + filter_domain);
        }

        if (domains.has_value()) {
            std::vector<std::string> included_domains{};
            std::vector<std::string> excluded_domains{};

            VERIFY(filter_list_process_domains(self, domains.value(), &included_domains, &excluded_domains));
            domain_vec.insert(domain_vec.end(), included_domains.begin(), included_domains.end());
            // We do not care about excluded domains here
        }

        if (!domain_vec.empty()) {
            if (generic_block) {
                self->generic_block_whitelist_filters.insert(
                    self->generic_block_whitelist_filters.end(),
                    domain_vec.begin(),
                    domain_vec.end()
                );
            } else {
                self->generic_hide_whitelist_filters.insert(
                    self->generic_hide_whitelist_filters.end(),
                    domain_vec.begin(),
                    domain_vec.end()
                );
            }
        }
    }

    if (*out_is_generic) {
        SUCCEED();
    }

    CreateFilterQuery query{
        .source = text,
        .content_type = content_type,
        .match_case = match_case,
        .domains = domains,
        .frame_urls = frame_urls,
        .third_party = third_party,
        .sitekeys = sitekeys,
    };

    VERIFY(filter_list_create_filter(self, query, out_item, out_ok));

    if (!blocking && *out_ok && out_item->find("action") != out_item->end()) {
        out_item->at("action")["type"] = "ignore-previous-rules";
    }

#if PRINT_JSON_FILTERS
    std::string json = rfl::json::write(*out_item);
    printf("%s\n", json.c_str());
#endif

    SUCCEED();
}

static Error
filter_list_parse_cosmetic_filter(
    FilterList* self, const std::string& text, CosmeticFilterInfo* out_filter_info, bool* out_ok)
{
    RAISE_CHECK(out_filter_info);
    RAISE_CHECK(out_ok);

    std::smatch match{};
    if (!std::regex_search(text, match, self->cosmetic_filter_regex)) {
        *out_ok = false;
        SUCCEED();
    }

    if (match.size() != 6) {
        RAISE(ErrorCode::assert, "unexpected regex match size %zu instead of %zu", match.size(), 6);
    }

    std::string domain = match[1].str();
    std::string is_exception = match[2].str();
    // std::string tag_name = match[3].str(); // unused
    // std::string attr_rules = match[4].str(); // unused
    std::string selector = match[5].str();

    static const std::vector<std::string> unsupported_selector_pseudos = {
        ":has-text",
        ":contains",
        ":matches-css",
        ":matches-attr",
        ":matches-path",
        ":matches-media",
        ":min-text-length",
        ":nth-ancestor",
        ":upward",
        ":xpath",
        ":watch-attr",
        ":remove",
        ":style",
        ":if",
        ":-abp-",
    };
    if (std::any_of(
            unsupported_selector_pseudos.begin(), unsupported_selector_pseudos.end(),
            [&selector](const std::string& pseudo) { return selector.find(pseudo) != std::string::npos; }
        )) {
        *out_ok = false;
        SUCCEED();
    }

    if (domain.empty()) {
        *out_filter_info = CosmeticFilterInfo{
            .selector = selector,
            .is_generic = true,
        };
        *out_ok = true;
        SUCCEED();
    }

    // Convert domain to lowercase in-place
    orion::str::to_lower(&domain);
    std::deque<std::string> domains = orion::str::split(domain, ',');
    std::vector<std::string> included_domains{};
    std::vector<std::string> excluded_domains{};

    for (const std::string& domain_item : domains) {
        const bool is_negated = domain_item.starts_with("~");
        const std::string raw_domain = is_negated ? domain_item.substr(1) : domain_item;
        std::string encoded_domain = raw_domain;
        if (!orion::str::is_ascii(raw_domain)) {
            encoded_domain = orion::punycode::encode(raw_domain);
        }
        if (is_negated) {
            excluded_domains.push_back("*" + encoded_domain);
        } else {
            included_domains.push_back("*" + encoded_domain);
        }
    }

    if (is_exception == "?" || is_exception == "$") {
        *out_ok = false;
        SUCCEED();
    }

    if (!included_domains.empty()) {
        *out_filter_info = CosmeticFilterInfo{
            .selector = selector,
            .domains = included_domains,
            .is_exception = !is_exception.empty(),
        };
    } else if (!excluded_domains.empty()) {
        *out_filter_info = CosmeticFilterInfo{
            .selector = selector,
            .domains = excluded_domains,
            .is_exception = true,
            .uses_unless_domain = true,
        };
    } else {
        *out_ok = false;
        SUCCEED();
    }
    
    *out_ok = true;
    SUCCEED();
}

static bool
filter_list_is_low_end_device(FilterList* self)
{
    if (!self->is_low_end_device.has_value()) {
        constexpr uint64_t memory_limit = 3ULL * 1024 * 1024 * 1024;
        self->is_low_end_device = uv_get_total_memory() <= memory_limit;
    }
    return self->is_low_end_device.value();
}

static Error
filter_list_add_filter(FilterList* self, const FilterListItem& item, std::vector<FilterListItem>* out_list)
{
    RAISE_CHECK(out_list);
    if (out_list->size() < FilterList::max_supported_rule_count) {
        out_list->push_back(item);
    }
    SUCCEED();
}

static Error
filter_list_parse_line(FilterList* self, const std::string& line, std::vector<FilterListItem>* out_list, bool* out_ok)
{
    RAISE_CHECK(out_list);
    RAISE_CHECK(out_ok);
    *out_ok = false;

    if (filter_list_is_low_end_device(self) && line.size() > 30) {
        SUCCEED();
    }

    if (line.find('#') != std::string::npos) {
        if (line.find("#%#") != std::string::npos || line.find("#$#") != std::string::npos
            || line.find("#?#") != std::string::npos || line.find("$$") != std::string::npos) {
            log_verbose("unsupported filter list rule #1: %s", line.c_str());
            SUCCEED();
        }

        std::smatch match{};
        bool is_match = std::regex_search(line, match, self->elemhide_regex);
        CosmeticFilterInfo filter_info{};
        bool ok = false;
        if (is_match) {
            VERIFY(filter_list_parse_cosmetic_filter(self, line, &filter_info, &ok));
        }

        if (is_match && ok) {
            if (filter_info.is_exception) {
                if (filter_info.uses_unless_domain) {
                    self->cosmetic_filters_with_if_top_domains_exceptions.push_back(filter_info);
                } else {
                    auto& domains = self->generic_cosmetic_exceptions[filter_info.selector];
                    domains.insert(filter_info.domains.begin(), filter_info.domains.end());
                }
            } else if (filter_info.is_generic) {
                self->generic_cosmetic_filters.push_back(filter_info.selector);
            } else {
                self->cosmetic_filters_with_if_top_domains.push_back(filter_info);
            }
        } else {
            log_verbose("unsupported filter list rule #1: %s", line.c_str());
            SUCCEED();
        }
    } else {
        FilterListItem item{};
        bool ok{};
        bool is_generic{};
        VERIFY(filter_list_parse_filter(self, line, &item, &ok, &is_generic));
        if (ok) {
            VERIFY(filter_list_add_filter(self, item, out_list));
        } else if (!is_generic) {
            log_verbose("unsupported filter list rule #2: %s", line.c_str());
            SUCCEED();
        }
    }

    *out_ok = true;
    SUCCEED();
}

static void
filter_list_append_extra_rules(std::deque<std::string>* lines)
{
    // Add extra rules you want to include to our lists here. If you want to exclude a rule that breaks
    // something find that rule in one of our lists then prefix it with `@@` characters.
    std::vector<std::string> extra_rules = {
		// Causes problems with adding JetBlue's boarding pass to Apple Wallet.
		"@@||urbanairship.com^",
		// Breaks opening search result links from Bing.
		"@@||bing.com/fd/ls/GLinkPing.aspx?",
    };

    for (const std::string& rule : extra_rules) {
        lines->push_back(rule);
    }
}

static std::optional<std::string>
filter_list_badfilter_target(const std::string& line)
{
    const size_t dollar_index = line.find('$');
    if (dollar_index == std::string::npos) {
        return std::nullopt;
    }

    std::deque<std::string> options = orion::str::split(line.substr(dollar_index + 1), ',');
    const auto is_badfilter = [](const std::string& option) {
        return filter_list_normalize_option(option) == "badfilter";
    };
    if (std::none_of(options.begin(), options.end(), is_badfilter)) {
        return std::nullopt;
    }

    std::vector<std::string> filtered{};
    for (const std::string& option : options) {
        if (!is_badfilter(option)) {
            filtered.push_back(option);
        }
    }

    const std::string base = line.substr(0, dollar_index);
    return filtered.empty() ? base : base + "$" + orion::str::join_strings(filtered, ",");
}

static std::string
filter_list_option_key(const std::string& option)
{
    const size_t separator_index = option.find('=');
    return filter_list_normalize_option(option.substr(0, separator_index));
}

static std::optional<std::vector<std::string>>
filter_list_expand_denyallow_rule(const std::string& line)
{
    const size_t dollar_index = line.find('$');
    if (dollar_index == std::string::npos) {
        return std::nullopt;
    }

    const std::deque<std::string> options = orion::str::split(line.substr(dollar_index + 1), ',');
    std::optional<std::string> denyallow_value{};
    bool has_domain = false;
    for (const std::string& option : options) {
        const std::string key = filter_list_option_key(option);
        if (key == "domain") {
            has_domain = true;
        }
        if (key == "denyallow" && !denyallow_value.has_value()) {
            const size_t separator_index = option.find('=');
            if (separator_index != std::string::npos && option.find('=', separator_index + 1) == std::string::npos) {
                denyallow_value = option.substr(separator_index + 1);
            }
        }
    }
    if (!denyallow_value.has_value() || denyallow_value->empty() || !has_domain) {
        return std::nullopt;
    }

    const std::deque<std::string> denyallow_domains = orion::str::split(denyallow_value.value(), '|');
    if (denyallow_domains.empty()
        || std::any_of(denyallow_domains.begin(), denyallow_domains.end(), [](const std::string& domain) {
            return domain.starts_with("~");
        })) {
        return std::nullopt;
    }

    const bool is_whitelist = line.starts_with("@@");
    const std::string line_without_prefix = is_whitelist ? line.substr(2) : line;
    const size_t pattern_dollar_index = line_without_prefix.find('$');
    if (pattern_dollar_index == std::string::npos) {
        return std::nullopt;
    }
    const std::string pattern = line_without_prefix.substr(0, pattern_dollar_index);

    std::vector<std::string> options_without_denyallow{};
    for (const std::string& option : options) {
        if (filter_list_option_key(option) != "denyallow") {
            options_without_denyallow.push_back(option);
        }
    }
    std::string clean_options = orion::str::join_strings(options_without_denyallow, ",");
    clean_options = replace_all(std::move(clean_options), "domain=", "frameurl=");

    std::vector<std::string> result{};
    result.push_back((is_whitelist ? "@@" : "") + pattern + "$" + clean_options);

    const bool is_generic_pattern = pattern == "*" || pattern.empty();
    for (const std::string& domain : denyallow_domains) {
        if (is_whitelist) {
            if (is_generic_pattern) {
                result.push_back("||" + domain + "$" + clean_options + ",important");
            } else {
                result.push_back("||" + domain + "/" + pattern + "$" + clean_options + ",important");
                result.push_back("||" + domain + "/*/" + pattern + "$" + clean_options + ",important");
            }
        } else if (is_generic_pattern) {
            result.push_back("@@||" + domain + "$" + clean_options);
        } else {
            result.push_back("@@||" + domain + "/" + pattern + "$" + clean_options);
            result.push_back("@@||" + domain + "/*/" + pattern + "$" + clean_options);
        }
    }
    return result;
}

static Error
filter_list_parse_lines(
    FilterList* self, const std::string& raw_data, std::stop_token stop_token, std::vector<FilterListItem>* out_list,
    int32_t* out_parsed_rule_count, std::string* out_version)
{
    RAISE_CHECK(out_parsed_rule_count);
    std::deque<std::string> lines = orion::str::split(raw_data, '\n');
    int line_num = 0;

    self->generic_cosmetic_filters.clear();
    self->cosmetic_filters_with_if_top_domains.clear();
    self->cosmetic_filters_with_if_top_domains_exceptions.clear();
    self->generic_cosmetic_exceptions.clear();
    self->generic_hide_whitelist_filters.clear();
    self->generic_block_whitelist_filters.clear();
    self->badfilter_patterns.clear();

    filter_list_append_extra_rules(&lines);

    const bool has_badfilter = std::any_of(lines.begin(), lines.end(), [](const std::string& line) {
        return line.find("badfilter") != std::string::npos;
    });
    if (has_badfilter) {
        for (const std::string& line : lines) {
            std::string trimmed_line = line;
            orion::str::trim(&trimmed_line);
            if (trimmed_line.empty() || trimmed_line.starts_with("!") || trimmed_line.starts_with("[")) {
                continue;
            }
            if (std::optional<std::string> target = filter_list_badfilter_target(trimmed_line)) {
                self->badfilter_patterns.insert(std::move(target.value()));
            }
        }
    }

    for (const std::string& line : lines) {
        if (stop_token.stop_requested()) {
            RAISE(ErrorCode::cancelled, "filter list parsing was cancelled");
        }

        line_num++;
        std::string trimmed_line = line;
        orion::str::trim(&trimmed_line);

        if (trimmed_line.starts_with("! Version:")) {
            if (out_version) {
                *out_version = trimmed_line.substr(std::string("! Version:").length());
                orion::str::trim(out_version);
            }
            continue;
        }

        if (trimmed_line.empty() || trimmed_line[0] == '!' || trimmed_line[0] == '[') {
            continue; // Skip empty lines, comments, and section headers
        }

        if (!self->badfilter_patterns.empty() && self->badfilter_patterns.contains(trimmed_line)) {
            continue;
        }

        if (self->is_easy_list && trimmed_line == R"(##[name^="google_ads_iframe"])") {
            continue;
        }
        if (self->is_easy_list && trimmed_line == R"(##[data-block-type="ad"])") {
            // Special filter from Wipr, which we couldn't found in any list.
            // specially `iframe[data-google-container-id]` and `iframe[title="3rd party ad content"]`
            trimmed_line = R"(~expressnews.com,~houstonchronicle.com,~myjournalcourier.com,~sfchronicle.com,~timesunion.com##[data-block-type="ad"],[name^="google_ads_iframe"],iframe[data-google-container-id],iframe[title="3rd party ad content"])";
        }

        std::vector<std::string> lines_to_parse{trimmed_line};
        if (trimmed_line.find("denyallow") != std::string::npos) {
            if (std::optional<std::vector<std::string>> expanded = filter_list_expand_denyallow_rule(trimmed_line)) {
                lines_to_parse = std::move(expanded.value());
            }
        }

        for (const std::string& expanded_line : lines_to_parse) {
            bool ok = false;
            Error err = filter_list_parse_line(self, expanded_line, out_list, &ok);
            if (err) {
                // Add line info to the error message for better debugging
                err.set_message(
                    std::format("failed to parse filter list at line {}: {}", line_num, err.message())
                );
                return WRAP_ERROR(err);
            }
            if (ok) {
                *out_parsed_rule_count += 1;
            }
        }
    }

    if (!self->generic_cosmetic_filters.empty()) {
        std::vector<std::string> normal_selectors{};
        struct ExceptionGroup {
            std::vector<std::string> domains{};
            std::vector<std::string> selectors{};
        };
        std::unordered_map<std::string, ExceptionGroup> exception_groups{};
        std::vector<std::string> group_order{};

        for (const std::string& selector : self->generic_cosmetic_filters) {
            auto exceptions_it = self->generic_cosmetic_exceptions.find(selector);
            if (exceptions_it == self->generic_cosmetic_exceptions.end() || exceptions_it->second.empty()) {
                normal_selectors.push_back(selector);
                continue;
            }

            std::vector<std::string> domains(exceptions_it->second.begin(), exceptions_it->second.end());
            const std::string key = orion::str::join_strings(domains, "\x1f");
            auto group_it = exception_groups.find(key);
            if (group_it != exception_groups.end()) {
                group_it->second.selectors.push_back(selector);
            } else {
                exception_groups.emplace(
                    key, ExceptionGroup{.domains = std::move(domains), .selectors = {selector}}
                );
                group_order.push_back(key);
            }
        }
        self->generic_cosmetic_filters.clear();

        for (size_t offset = 0; offset < normal_selectors.size(); offset += FilterList::max_selectors_per_rule) {
            // Use paranthesized (std::min) to avoid breaking Windows compilation
            const size_t end = (std::min)(offset + FilterList::max_selectors_per_rule, normal_selectors.size());
            std::vector<std::string> selectors(normal_selectors.begin() + offset, normal_selectors.begin() + end);
            CosmeticFilterInfo info{
                .selector = orion::str::join_strings(selectors, ", "),
            };
            VERIFY(filter_list_add_filter(self, cosmetic_filter_info_to_rule(info), out_list));
        }

        for (const std::string& key : group_order) {
            ExceptionGroup& group = exception_groups.at(key);
            for (size_t offset = 0; offset < group.selectors.size(); offset += FilterList::max_selectors_per_rule) {
                // Use paranthesized (std::min) to avoid breaking Windows compilation
                const size_t end = (std::min)(offset + FilterList::max_selectors_per_rule, group.selectors.size());
                std::vector<std::string> selectors(group.selectors.begin() + offset, group.selectors.begin() + end);
                FilterListItem item{};
                item["trigger"] = {
                    {"url-filter", ".*"},
                    {"unless-domain", group.domains},
                };
                item["action"] = {
                    {"type", "css-display-none"},
                    {"selector", orion::str::join_strings(selectors, ", ")},
                };
                VERIFY(filter_list_add_filter(self, item, out_list));
            }
        }
    }

    if (!self->cosmetic_filters_with_if_top_domains.empty()) {
        std::vector<FilterListItem> rules = optimized_rules(
            self->cosmetic_filters_with_if_top_domains, false, self->max_selectors_per_rule
        );
        for (const FilterListItem& item : rules) {
            VERIFY(filter_list_add_filter(self, item, out_list));
        }
    }

    if (!self->cosmetic_filters_with_if_top_domains_exceptions.empty()) {
        std::vector<FilterListItem> rules = optimized_rules(
            self->cosmetic_filters_with_if_top_domains_exceptions, true, self->max_selectors_per_rule
        );
        for (const FilterListItem& item : rules) {
            VERIFY(filter_list_add_filter(self, item, out_list));
        }
    }

    if (!self->generic_hide_whitelist_filters.empty()) {
        FilterListItem item{};
        item["trigger"]["url-filter"] = ".*";
        item["trigger"]["if-domain"] = self->generic_hide_whitelist_filters;
        item["action"]["type"] = "ignore-previous-rules";
        VERIFY(filter_list_add_filter(self, item, out_list));
    }

    if (!self->generic_block_whitelist_filters.empty()) {
        FilterListItem item{};
        item["trigger"]["url-filter"] = ".*";
        item["trigger"]["if-domain"] = self->generic_block_whitelist_filters;
        item["action"]["type"] = "ignore-previous-rules";
        VERIFY(filter_list_add_filter(self, item, out_list));
    }

    SUCCEED();
}

FilterList*
filter_list_new(const char* list_category)
{
    auto* self = new FilterList();
    if (list_category) {
        std::string normalized_category = list_category;
        orion::str::to_lower(&normalized_category);
        self->is_easy_list = normalized_category == "easylist";
    }
    return self;
}

FilterList*
filter_list_new()
{
    return filter_list_new(nullptr);
}

void
filter_list_free(FilterList* self)
{
    delete self;
}

static Error
filter_list_parse_metadata(const char* raw_data, std::string_view prefix, std::string* out_value)
{
    if (!raw_data) {
        RAISE(ErrorCode::argument, "raw_data is null");
    }
    RAISE_CHECK(out_value);

    // This could be optimized in the future if needed
    std::deque<std::string> lines = orion::str::split(raw_data, '\n');

    for (const std::string& line : lines) {
        std::string trimmed_line = line;
        orion::str::trim(&trimmed_line);

        if (trimmed_line.starts_with(prefix)) {
            *out_value = trimmed_line.substr(prefix.length());
            orion::str::trim(out_value);
            SUCCEED();
        }
    }

    *out_value = "";
    SUCCEED();
}

Error
filter_list_parse_version(FilterList* self, const char* raw_data, std::string* out_version)
{
    return filter_list_parse_metadata(raw_data, "! Version:", out_version);
}

Error
filter_list_parse_title(FilterList* self, const char* raw_data, std::string* out_title)
{
    return filter_list_parse_metadata(raw_data, "! Title:", out_title);
}

Error
filter_list_parse_description(FilterList* self, const char* raw_data, std::string* out_description)
{
    return filter_list_parse_metadata(raw_data, "! Description:", out_description);
}

Error
filter_list_parse(
    FilterList* self, const char* raw_data, std::stop_token stop_token, std::vector<FilterListItem>* out_items,
    int32_t* out_parsed_rule_count, std::string* out_version)
{
    if (!raw_data) {
        RAISE(ErrorCode::argument, "raw_data is null");
    }
    RAISE_CHECK(out_items);
    RAISE_CHECK(out_parsed_rule_count);

    if (out_version) {
        *out_version = "";
    }

    self->resource_type_prototype = self->resource_type_prototype & ~(
        ResourceType::document | ResourceType::subdocument | ResourceType::elemhide | ResourceType::popup
    );

    // Note:
    // Some symbols for example @, /, _, !, = (and some others) do not have to be escaped in the regexes below when
    // appearing inside [...], but  we escape them (without changing the meaning) to keep the Swift and C++
    // versions closer.

    // Note: 'zzz' tokens below are just C++ custom string delimiters

    self->cosmetic_filter_regex = std::regex(
        R"zzz(^([^\/\*\|\@"!]*?)#([\@\?])?(?:([\w\-]+|\*)((?:\([\w\-]+(?:[$^*]?=[^\(\)"]*)?\))*)|#([^{}]+))$)zzz"
    );
    self->quantifier_regex = std::regex(
        R"zzz(\{0*[1-9]\d*\}|\{0*[1-9]\d*,\}|\{[0-9]\d*,0*[0-9]\d*\})zzz"
    );
    self->elemhide_regex = std::regex(
        R"zzz(^([^\|@"!]*?)#(\@|\?|\$)?(?:([\w\-]+|\*)((?:\([\w\-]+(?:[$^*]?=[^\(\)"]*)?\))*)|#([^{}]+))$)zzz"
    );
    self->group_regex = std::regex(
        R"zzz(\(\?[\!\=].*\))zzz"
    );

    self->generic_cosmetic_filters.clear();
    self->cosmetic_filters_with_if_top_domains.clear();
    self->cosmetic_filters_with_if_top_domains_exceptions.clear();
    self->generic_cosmetic_exceptions.clear();
    self->generic_hide_whitelist_filters.clear();
    self->generic_block_whitelist_filters.clear();
    self->badfilter_patterns.clear();

    VERIFY(filter_list_parse_lines(self, raw_data, stop_token, out_items, out_parsed_rule_count, out_version));
    SUCCEED();
}

} // namespace orion
