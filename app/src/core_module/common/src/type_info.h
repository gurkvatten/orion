#pragma once
#include <string>

namespace orion {

struct TypeInfo {
    static std::string type_name(std::string mangled);
    static std::string strip_ns(std::string name);
};

}