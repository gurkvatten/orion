#include <core/filter_list_metadata.h>

#ifndef _
#define _(text) text
#endif

#ifndef GRESOURCE_PREFIX
#define GRESOURCE_PREFIX ""
#endif

namespace orion::filter_lists {

static const std::vector<Metadata> global_metadatas{
    {
        .name = "easylist",
        .display_name = "EasyList",
        .resource_path = GRESOURCE_PREFIX "/filter_lists/easylist.txt",
        .url = "https://easylist.to/easylist/easylist.txt",
        .is_data_embedded = true,
        .description = _("EasyList is the primary filter list that removes most adverts from international webpages, including unwanted frames, images and objects. This filter is necessary for quality ad blocking"),
        .is_enabled = true,
        .position = 0,
        .filter_type = FilterListType::builtin,
    },
    {
        .name = "easyprivacy",
        .display_name = "EasyPrivacy",
        .resource_path = GRESOURCE_PREFIX "/filter_lists/easyprivacy.txt",
        .url = "https://easylist.to/easylist/easyprivacy.txt",
        .is_data_embedded = true,
        .description = _("EasyPrivacy is supplementary filter list that completely removes all forms of tracking from the internet, thereby protecting your personal data."),
        .is_enabled = true,
        .position = 1,
        .filter_type = FilterListType::builtin,
    },
    {
        // Unfortunately, this list does not contain version info in its text data
        .name = "specific",
        .display_name = "AdGuard Base (Site-Specific)",
        .resource_path = GRESOURCE_PREFIX "/filter_lists/specific.txt",
        .url = "https://raw.githubusercontent.com/AdguardTeam/AdguardFilters/master/BaseFilter/sections/specific.txt",
        .version = "2026-07-30",
        .is_data_embedded = true,
        .description = _("AdGuard's site-specific rules block ads and fix layout issues on thousands of individual websites. Complements EasyList by covering cases that generic rules don't reach; safe to enable alongside it."),
        .is_enabled = true,
        .position = 2,
        .filter_type = FilterListType::builtin,
    },
    // Removed in Mac Orion version
    // {
    //     .name = "light",
    //     .display_name = "Hagezi Light DNS Blocklist",
    //     .resource_path = GRESOURCE_PREFIX "/filter_lists/light.txt",
    //     .url = "https://raw.githubusercontent.com/hagezi/dns-blocklists/main/adblock/light.txt",
    //     .is_data_embedded = false,
    //     .description = _("Blocks domains associated with ads, tracking, analytics, and badware. More exhaustive than the Light list. Only need to enable one: Light or Pro Plus."),
    //     .is_enabled = false,
    // },
    {
        .name = "fanboy-annoyance",
        .display_name = "Fanboy’s Annoyance",
        .resource_path = GRESOURCE_PREFIX "/filter_lists/fanboy-annoyance.txt",
        .url = "https://secure.fanboy.co.nz/fanboy-annoyance.txt",
        .is_data_embedded = false,
        .description = _("Fanboy Annoyances list blocks social media content, cookies banners, GDPR overlay windows, in page Pop-Ups and other annoyances. It includes EasyList Cookie List."),
        .is_enabled = false,
        .position = 3,
        .filter_type = FilterListType::builtin,
    },
    {
        .name = "pro-plus-mini",
        .display_name = "Hagezi Pro Plus Mini",
        .resource_path = GRESOURCE_PREFIX "/filter_lists/pro.plus.mini.txt",
        .url = "https://raw.githubusercontent.com/hagezi/dns-blocklists/main/adblock/pro.plus.mini.txt",
        .is_data_embedded = false,
        .description = _("Blocks domains associated with ads, tracking, analytics, and badware. More exhaustive than the Light list. Only need to enable one: Light or Pro Plus."),
        .is_enabled = false,
        .position = 4,
        .filter_type = FilterListType::builtin,
    },
    {
        .name = "privacy-essentials",
        .display_name = "Privacy Essentials",
        .resource_path = GRESOURCE_PREFIX "/filter_lists/privacy_essentials.txt",
        .url = "https://raw.githubusercontent.com/yokoffing/filterlists/main/privacy_essentials.txt",
        .is_data_embedded = false,
        .description = _("An opinionated, curated list for advanced hardening. May break third-party logins."),
        .is_enabled = false,
        .position = 5,
        .filter_type = FilterListType::builtin,
    },
    {
        .name = "block-third-party-fonts",
        .display_name = "Block third-party fonts",
        .resource_path = GRESOURCE_PREFIX "/filter_lists/block_third_party_fonts.txt",
        .url = "https://raw.githubusercontent.com/yokoffing/filterlists/main/block_third_party_fonts.txt",
        .is_data_embedded = false,
        .description = _("A successor to Fanboy’s Anti-third party Fonts, this filter list blocks most web fonts from third-parties when possible. A lighter option when compared to blocking all web fonts."),
        .is_enabled = false,
        .position = 6,
        .filter_type = FilterListType::builtin,
    },
    // {
    //     .name = "orion-test-list",
    //     .raw_data = R"(
    //         /addyn|*;adtech;
    //         /(https?:\/\/)255\.254\..{100,}/
    //         ! /sdn.cz/d_.*jpeg/
    //         ! /sdn.cz/d_.*png/
    //         ! /sdn.cz/d_.*webp/
    //     )",
    //     .is_data_embedded = false,
    // },
};

const std::vector<Metadata>&
get_metadata_list()
{
    return global_metadatas;
}

} // namespace orion::filter_lists
