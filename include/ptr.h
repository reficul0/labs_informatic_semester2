#ifndef PTR_H
#define PTR_H

#include <memory>

template<typename T>
struct ptr : std::shared_ptr<T>
{
    template<typename ... Args>
    ptr(Args&& ... args)
        : std::shared_ptr<T>(std::forward<Args>(args)...)
    {

    }
};

#endif // PTR_H
