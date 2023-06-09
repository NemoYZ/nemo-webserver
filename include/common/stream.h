#pragma once

#include <memory>

#include "container/byte_array.h"

namespace nemo {

/**
 * @brief 流结构
 */
class Stream {
public:
    typedef std::shared_ptr<Stream> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<Stream> UniquePtr; ///< 智能指针定义
    
    /**
     * @brief 析构函数
     */
    virtual ~Stream() = default;

    /**
     * @brief 读数据
     * @param[out] buffer 接收数据的内存
     * @param[in] length 接收数据的内存大小
     * @return
     *      @retval >0 返回接收到的数据的实际大小
     *      @retval =0 被关闭
     *      @retval <0 出现流错误
     */
    virtual int read(void* buffer, size_t length) = 0;

    /**
     * @brief 读数据
     * @param[out] byteArray 接收数据的ByteArray
     * @param[in] length 接收数据的内存大小
     * @return
     *      @retval >0 返回接收到的数据的实际大小
     *      @retval =0 被关闭
     *      @retval <0 出现流错误
     */
    virtual int read(ByteArray* byteArray, size_t length) = 0;

    /**
     * @brief 读固定长度的数据
     * @param[out] buffer 接收数据的内存
     * @param[in] length 接收数据的内存大小
     * @return
     *      @retval >0 返回接收到的数据的实际大小
     *      @retval =0 被关闭
     *      @retval <0 出现流错误
     */
    virtual int readFixSize(void* buffer, size_t length);

    /**
     * @brief 读固定长度的数据
     * @param[out] byteArray 接收数据的ByteArray
     * @param[in] length 接收数据的内存大小
     * @return
     *      @retval >0 返回接收到的数据的实际大小
     *      @retval =0 被关闭
     *      @retval <0 出现流错误
     */
    virtual int readFixSize(ByteArray* byteArray, size_t length);

    /**
     * @brief 写数据
     * @param[in] buffer 写数据的内存
     * @param[in] length 写入数据的内存大小
     * @return
     *      @retval >0 返回写入到的数据的实际大小
     *      @retval =0 被关闭
     *      @retval <0 出现流错误
     */
    virtual int write(const void* buffer, size_t length) = 0;

    /**
     * @brief 写数据
     * @param[in] byteArray 写数据的ByteArray
     * @param[in] length 写入数据的内存大小
     * @return
     *      @retval >0 返回写入到的数据的实际大小
     *      @retval =0 被关闭
     *      @retval <0 出现流错误
     */
    virtual int write(ByteArray* bytearray, size_t length) = 0;

    /**
     * @brief 写固定长度的数据
     * @param[in] buffer 写数据的内存
     * @param[in] length 写入数据的内存大小
     * @return
     *      @retval >0 返回写入到的数据的实际大小
     *      @retval =0 被关闭
     *      @retval <0 出现流错误
     */
    virtual int writeFixSize(const void* buffer, size_t length);

    /**
     * @brief 写固定长度的数据
     * @param[in] byteArray 写数据的ByteArray
     * @param[in] length 写入数据的内存大小
     * @return
     *      @retval >0 返回写入到的数据的实际大小
     *      @retval =0 被关闭
     *      @retval <0 出现流错误
     */
    virtual int writeFixSize(ByteArray* bytearray, size_t length);

    /**
     * @brief 关闭流
     */
    virtual void close() = 0;
};

} // namespace nemo