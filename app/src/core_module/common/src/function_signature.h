#pragma once

#ifdef _MSC_VER
    #define FUNCTION_SIGNATURE __FUNCTION__
#else
    #define FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#endif