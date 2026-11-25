#pragma once

namespace orion {

// Visitor can be used when working with std::variant to handle multiple types more
// gracefully using lambdas, for example:
//    return std::visit(Visitor{
//        [](std::nullptr_t) { return std::string("NULL"); },
//        [](const std::string& str) { return "\"" + str + "\""; },
//        [](int64_t i) { return std::to_string(i); },
//        [](double d) { return std::to_string(d); },
//        [](const std::vector<uint8_t>& blob) { return "BLOB(" + std::to_string(blob.size()) + ")"; },
//        [](const auto& val) { return std::to_string(val); },
//    }, value);
template<class... Ts> struct Visitor : Ts... { using Ts::operator()...; };
template<class... Ts> Visitor(Ts...) -> Visitor<Ts...>;

} // namespace orion