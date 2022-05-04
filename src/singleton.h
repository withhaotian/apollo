#ifndef __APOLLO_SINGLETON_H__
#define __APOLLO_SINGLETON_H__

#include <memory>

namespace apollo {

/**
 * @brief   单例模式封装类
 * @details T 类型
 *          X 为了创造多个实例对应的Tag
 *          N 同一个Tag创造多个实例索引
 */
template<class T, class X = void, int N = 0>
class Singleton {
public:
    static T* GetInstance() {
        static T v;
        return &v;
    }
private:
};

/**
 * @brief 单例模式智能指针封装类
 * @details T 类型
 *          X 为了创造多个实例对应的Tag
 *          N 同一个Tag创造多个实例索引
 */
template<class T, class X = void, int N = 0>
class SingletonPtr {
public:
    static std::shared_ptr<T> GetInstancePtr() {
        static std::shared_ptr<T> v(new T);
        return v;
    }
private:
};

}

#endif  // SINGLETON_H