#include <chrono>
#include <src/time_util.h>

namespace orion::tm {

using namespace std::chrono;

UnixTimeUtc64
unix_time_utc64_now()
{
    auto now = system_clock::now();
    auto now_secs = time_point_cast<seconds>(now);
    return UnixTimeUtc64(now_secs.time_since_epoch().count());
}

static auto
to_local(time_point<system_clock, seconds> utc)
{
    auto zone = current_zone();
    auto local = zoned_time{zone, utc};
    auto local_t = local.get_local_time();
    return local_t;
}

int64_t
today_local()
{
    auto utc_now = floor<seconds>(system_clock::now());
    auto local_t = to_local(utc_now);
    auto daysp = floor<days>(seconds(local_t.time_since_epoch().count())).count();
    return daysp;
}

bool
is_same_day(UnixTimeUtc64 t1, UnixTimeUtc64 t2)
{
    auto cht1 = floor<days>(seconds(t1.value));   
    auto cht2 = floor<days>(seconds(t2.value));
    return floor<days>(cht1) == floor<days>(cht2);
}

bool
is_today_utc(UnixTimeUtc64 t)
{
    auto zone = locate_zone("UTC");
    auto local = zone->to_local(system_clock::now());
    auto dayp = floor<days>(local.time_since_epoch());
    auto t_dayp = floor<days>(seconds(t.value));
    return t_dayp == dayp;
}

int64_t
to_day(UnixTimeUtc64 t)
{
    auto t_dayp = floor<days>(seconds(t.value));
    return t_dayp.count();
}

int64_t
duration_seconds(UnixTimeUtc64 t1, UnixTimeUtc64 t2)
{
    return t1.value - t2.value;
}

} // namespace orion::tm