
#pragma once
#include <thread>

namespace orion {

void set_current_thread_name( const char* threadName);
void set_thread_name(std::thread* thread, const char* threadName);

} //namespace orion
