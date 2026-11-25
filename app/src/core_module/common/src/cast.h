#pragma once

#include <memory>

template<typename T, typename Y>
T*
cast(Y* ptr)
{
    return static_cast<T*>(ptr);
}

template<typename T, typename Y>
T*
cast(const std::shared_ptr<Y>& object)
{
    if(!object) {
        return nullptr;
    }
    return cast<T, Y>(object.get());
}

template<typename T, typename Y>
T*
cast(const std::weak_ptr<Y>& weakptr)
{
    return cast<T, Y>(weakptr.lock());
}

template<typename T, typename Y>
std::function<void()>
weakcall(std::weak_ptr<Y> runtime, const std::function<void()>& fn)
{
    return [runtime,fn](){
        if(cast<T, Y>(runtime)) {
            fn();
        }
    };
}

template<typename T, typename Y>
std::function<void()>
weakcall(std::weak_ptr<Y> runtime, T*, const std::function<void()>& fn)
{
    return [runtime,fn](){
        if(cast<T, Y>(runtime)) {
            fn();
        }
    };
}
