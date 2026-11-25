#include <src/log.h>

#include <iostream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <stdarg.h>

namespace orion {

static std::mutex g_log_mutex;
static LogSink g_log_sink = nullptr;

void
set_log_sink(LogSink sink)
{
	std::lock_guard<std::mutex> lock(g_log_mutex);
	g_log_sink = sink;
}

static std::string
timestamp_string()
{
	auto now = std::chrono::system_clock::now();

	// Get microsecond part
	auto days = std::chrono::time_point_cast<std::chrono::duration<int, std::ratio<86400>>>(now);
	auto time = now - days;
	auto h = std::chrono::duration_cast<std::chrono::hours>(time);
	time -= h;
	auto m = std::chrono::duration_cast<std::chrono::minutes>(time);
	time -= m;
	auto s = std::chrono::duration_cast<std::chrono::seconds>(time);
	time -= s;
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
	
	auto tt = std::chrono::system_clock::to_time_t(now);

	struct tm local_buf;
#ifdef _MSC_VER
	localtime_s(&local_buf, &tt);
#else
	localtime_r(&tt, &local_buf);
#endif
	auto* local_tm = &local_buf;

    std::stringstream ss;

	ss
		/*<< std::setw(4) << std::setfill('0') << (local_tm->tm_year + 1900) << "-"
		<< std::setw(2) << std::setfill('0') << (local_tm->tm_mon + 1) << "-"
		<< std::setw(2) << std::setfill('0') << local_tm->tm_mday
		<< " "*/
		<< std::setw(2) << std::setfill('0') << local_tm->tm_hour << ":"
		<< std::setw(2) << std::setfill('0') << local_tm->tm_min << ":"
		<< std::setw(2) << std::setfill('0') << local_tm->tm_sec << "."
		<< std::setw(6) << std::setfill('0') << us.count();

    return ss.str();
}

static std::string
format_to_string(const char* format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    std::string result(size, '\0');
    vsnprintf(&result[0], size + 1, format, args);
    return result;
}

static void
output_line(const std::string& line)
{
	std::lock_guard<std::mutex> lock(g_log_mutex);
	if (g_log_sink)
		g_log_sink(line.c_str());
	else
		printf("%s", line.c_str());
}

void
log(const std::string& level, const char* format, va_list args)
{
    std::string s = format_to_string(format, args) + '\n';
	std::string line = timestamp_string() + "[" + level + "] " + s;
	output_line(line);
}

void
log_message(char level, const char* message)
{
	std::string line = timestamp_string() + "[" + std::string(1, level) + "] " + message + "\n";
	output_line(line);
}

void
log_info(const char* format, ...)
{
	va_list args{};
	va_start(args, format);
	log("I", format, args);
	va_end(args);
}

void
log_error(const char* format, ...)
{
	va_list args{};
	va_start(args, format);
	log("E", format, args);
	va_end(args);
}

void
log_error(const std::string& message)
{
    log_error("%s", message.c_str());
}

void
log_warning(const char* format, ...)
{
	va_list args{};
	va_start(args, format);
	log("W", format, args);
	va_end(args);
}

void
log_debug(const char* format, ...)
{
#ifndef NDEBUG
	va_list args{};
	va_start(args, format);
	log("D", format, args);
	va_end(args);
#endif
}

void
internal_log_verbose(const char* format, ...)
{
	va_list args{};
	va_start(args, format);
	log("V", format, args);
	va_end(args);
}

} // namespace orion