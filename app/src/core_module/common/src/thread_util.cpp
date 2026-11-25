#include <src/thread_util.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace orion {

#ifdef _WIN32

static HRESULT (WINAPI *set_thread_description)(HANDLE, PCWSTR);
static bool find_set_thread_description_tried = false;

void find_set_thread_description()
{
    if (find_set_thread_description_tried)
        return;
    find_set_thread_description_tried = true;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32)
        set_thread_description = reinterpret_cast<decltype(set_thread_description)>(
            GetProcAddress(kernel32, "SetThreadDescription"));
#endif
}

//if that fails we can try the "old"way below using the exception. For that  we just need to convert wchar_t* to char*.
bool set_current_thread_name(const wchar_t *const name) {
    find_set_thread_description();
    if (set_thread_description) {/* Only available since Windows 10 1607 */
        set_thread_description(GetCurrentThread(), name);
        return true;
    }
    return false;
}

const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

//this method should first try the set_current_thread_name above and if that fails it should convert char* to wchar_t* and fallthrough.
void set_thread_name(uint32_t dwThreadID, const char* threadName)
{

  // DWORD dwThreadID = ::GetThreadId( static_cast<HANDLE>( t.native_handle() ) );

   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = dwThreadID;
   info.dwFlags = 0;

   __try
   {
      RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }
}
void set_current_thread_name( const char* threadName)
{
    set_thread_name(GetCurrentThreadId(),threadName);
}

void set_thread_name( std::thread* thread, const char* threadName)
{
    DWORD threadId = ::GetThreadId( static_cast<HANDLE>( thread->native_handle() ) );
    set_thread_name(threadId,threadName);
}

#elif defined(__linux__)
#include <sys/prctl.h>
void set_current_thread_name( const char* threadName)
{
  prctl(PR_SET_NAME,threadName,0,0,0);
}

void set_thread_name(std::thread* thread, const char* threadName)
{
   auto handle = thread->native_handle();
   pthread_setname_np(handle,threadName);
}

#elif defined(__APPLE__)
#include <pthread.h>
void set_current_thread_name(const char *name)
{
    pthread_setname_np(name);
}

void set_thread_name(std::thread* thread, const char* threadName)
{
    //noop, since on macOS you can only set the current thread's name.
}
#else
void set_thread_name(std::thread* thread, const char* threadName)
{
   auto handle = thread->native_handle();
   pthread_setname_np(handle,threadName);
}

void set_current_thread_name(const char *name)
{
    pthread_setname_np(std::this_thread::get_id(), name);
}

#endif


} //namespace orion