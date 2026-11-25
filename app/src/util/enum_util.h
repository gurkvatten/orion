#pragma once

#include <type_traits>
#include <core/dto/settings_dto.h>

#include "app.h"

namespace orion::enums {

//likely unnecessary since the code that requires this expects very specific values anyway, oh well
template <typename T>
auto to_underlying(T t) noexcept
{
    return static_cast<std::underlying_type_t<T>>(t);
}


// template <typename EnumT, EnumT TargetValue, StaticString DefaultStr>
// consteval auto build_constexpr_enum_map() {
    
//     // Unpack the enumerators using apply and a variadic lambda
//     return rfl::get_enumerators<EnumT>().apply([](const auto&... fields) {
        
//         // Create an array sized exactly to the number of enumerators
//         std::array<std::pair<EnumT, std::string_view>, sizeof...(fields)> map{};
//         size_t index = 0;
        
//         // Use a fold expression to iterate over each field at compile time
//         ([&](const auto& field) {
//             if (field.value() == TargetValue) {
//                 map[index] = {field.value(), DefaultStr.view()};
//             } else {
//                 map[index] = {field.value(), field.name()};
//             }
//             index++;
//         }(fields), ...);
        
//         return map;
//     });
// }


}