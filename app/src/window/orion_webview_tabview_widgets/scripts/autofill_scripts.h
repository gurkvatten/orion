#pragma once

#include <string>

namespace orion::gtk {

std::string get_autofill_script();
std::string get_input_focus_script();
std::string get_form_submit_script();
std::string get_scrape_username_password_script();

} // namespace orion::gtk