#ifndef __APOLLO_NONCOPYABLE_H__
#define __APOLLO_NONCOPYABLE_H__

namespace apollo
{

// 基类 - 禁用对象拷贝、复制
class Noncopyable {
public:
    /**
     * 默认构造函数
     */
    Noncopyable() = default;

    /**
     * 默认析构函数
     */
    ~Noncopyable() = default;

    /**
     * 拷贝构造函数(禁用)
     */
    Noncopyable(const Noncopyable&) = delete;

    /**
     * 赋值函数(禁用)
     */
    Noncopyable& operator=(const Noncopyable&) = delete;
};

} // namespace apllo


#endif      // NONCOPYABLE_Hs