#pragma once

#include <string>
#include <format>

#include <common/src/string_util.h>

namespace orion::gtk {

std::string get_reader_epoch_script(const std::string_view message_handler);
std::string get_page_tweaks_epoch_script(const std::string_view message_handler);
std::string run_against_epoch(const std::string_view script, const std::string_view epoch);

} // namespace orion::gtk