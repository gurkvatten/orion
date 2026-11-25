/*
 * export.h -- header file for export/import macros
 *
 */
#ifndef _EXPORT_H_
#define _EXPORT_H_
#if _WIN32
#ifdef CORE_MODULE_DLL_EXPORTS
    #define CORE_MODULE_API __declspec(dllexport)
#else
    #define CORE_MODULE_API __declspec(dllimport)
#endif
#else
    #define CORE_MODULE_API
#endif

#endif /* _EXPORT_H_ */
