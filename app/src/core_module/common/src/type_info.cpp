
#include "type_info.h"

#ifdef _WIN32
// TODO: this needs further investigation, how to implement for WIN32
/*
extern "C" {
    char* __unDName(char* demangled, const char* mangled, int out_len, 
        void* (*pAlloc)(size_t), void (*pFree)(void*),
        unsigned short int flags);
}
// Alternatively
#include <dbghelp.h>
*/
#else
#include <cxxabi.h>
#endif

namespace orion {
//https://stackoverflow.com/questions/1055452/c-get-name-of-type-in-template
std::string TypeInfo::type_name(std::string mangled) {
    int status{ 1 };
#ifdef _WIN32
    // TODO: this needs further investigation, how to implement for WIN32
    char *demangled_name = NULL; // __unDName(0, mangled.c_str(), 0, malloc, free, UNDNAME_COMPLETE);
#else
    char *demangled_name = abi::__cxa_demangle(mangled.c_str(), NULL, NULL, &status);
#endif
    if(status == 0) {
        mangled = demangled_name;
        std::free(demangled_name);
    }

    return mangled;
}

std::string TypeInfo::strip_ns(std::string name) {
    size_t pos = name.find_last_of(':');
    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }
    return name;
};
}
