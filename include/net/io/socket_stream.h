#pragma once

#include <memory>

#include "common/stream.h"
#include "net/socket.h"

namespace nemo {
namespace net {
namespace io {

/**
 * @brief Socket流
 */
class SocketStream : public Stream {
public:
    typedef std::shared_ptr<SocketStream> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<SocketStream> UniquePtr; ///< 智能指针定义

public:
    /**
     * @brief 构造函数
     * @param[in] sock Socket类
     * @param[in] owner 是否完全控制socket
     */
    SocketStream(Socket* sock);

    SocketStream(Socket::UniquePtr&& sock);

    /**
     * @brief 析构函数
     * @details 如果mOwner=true,则close
     */
    ~SocketStream();

    /**
     * @brief 读取数据
     * @param[out] buffer 待接收数据的内存
     * @param[in] length 待接收数据的内存长度
     * @return
     *      @retval >0 返回实际接收到的数据长度
     *      @retval =0 socket被远端关闭
     *      @retval <0 socket错误
     */
    virtual int read(void* buffer, size_t length) override;

    /**
     * @brief 读取数据
     * @param[out] bytearray 接收数据的ByteArray
     * @param[in] length 待接收数据的内存长度
     * @return
     *      @retval >0 返回实际接收到的数据长度
     *      @retval =0 socket被远端关闭
     *      @retval <0 socket错误
     */
    virtual int read(ByteArray* bytearray, size_t length) override;

    /**
     * @brief 写入数据
     * @param[in] buffer 待发送数据的内存
     * @param[in] length 待发送数据的内存长度
     * @return
     *      @retval >0 返回实际接收到的数据长度
     *      @retval =0 socket被远端关闭
     *      @retval <0 socket错误
     */
    virtual int write(const void* buffer, size_t length) override;

    /**
     * @brief 写入数据
     * @param[in] bytearray 待发送数据的ByteArray
     * @param[in] length 待发送数据的内存长度
     * @return
     *      @retval >0 返回实际接收到的数据长度
     *      @retval =0 socket被远端关闭
     *      @retval <0 socket错误
     */
    virtual int write(ByteArray* bytearray, size_t length) override;

    /**
     * @brief 关闭socket
     */
    virtual void close() override;

    /**
     * @brief 返回Socket类
     */
    Socket* getSocket() const { return socket_.get(); }

    /**
     * @brief 返回是否连接
     */
    bool isConnect() const {
        return socket_ && socket_->isConnect();
    }

    /**
     * @brief 获取远端地址
     */
    Address* getRemoteAddress();

    /**
     * @brief 获取本地地址
     */
    Address* getLocalAddress();

    /**
     * @brief 以字符串的形式获取远端地址
     */
    String getRemoteAddressString();

    /**
     * @brief 以字符串的形式获取本地地址
     */
    String getLocalAddressString();

protected:
    Socket::UniquePtr socket_; ///< Socket类
    bool owner_;               ///< 是否拥有socket的控制权
};

} // namespace io
} // namespace net
} // namespace nemo