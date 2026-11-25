#pragma once

#define UNIQUE_PTR(T, Dtor) std::unique_ptr<T, decltype([](T* p){ Dtor(p); })>
