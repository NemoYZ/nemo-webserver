#pragma once

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <memory>

#include "net/address.h"
#include "common/noncopyable.h"

namespace nemo {
namespace net {

/**
 * @brief Socket封装类
 */
class Socket : public std::enable_shared_from_this<Socket>, Noncopyable {
public:
    typedef std::shared_ptr<Socket> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<Socket> UniquePtr; ///< 智能指针定义

    /**
     * @brief 创建TCP Socket(满足地址类型)
     * @param[in] address 地址
     */
    static Socket::UniquePtr CreateTcp(const Address* address);

    /**
     * @brief 创建UDP Socket(满足地址类型)
     * @param[in] address 地址
     */
    static Socket::UniquePtr CreateUdp(const Address* address);

    /**
     * @brief 创建IPv4的TCP Socket
     */
    static Socket::UniquePtr CreateTcpSocket();

    /**
     * @brief 创建IPv4的UDP Socket
     */
    static Socket::UniquePtr CreateUdpSocket();

    /**
     * @brief 创建IPv6的TCP Socket
     */
    static Socket::UniquePtr CreateTcpSocket6();

    /**
     * @brief 创建IPv6的UDP Socket
     */
    static Socket::UniquePtr CreateUdpSocket6();

    /**
     * @brief 创建Unix的TCP Socket
     */
    static Socket::UniquePtr CreateUnixTcpSocket();

    /**
     * @brief 创建Unix的UDP Socket
     */
    static Socket::UniquePtr CreateUnixUdpSocket();

    /**
     * @brief Socket构造函数
     * @param[in] family 协议簇
     * @param[in] type 类型
     * @param[in] protocol 协议
     */
    Socket(int family, int type, int protocol = 0);

    Socket(const SocketAttribute& attribute);

    /**
     * @brief 析构函数
     */
    virtual ~Socket();

    /**
     * @brief 获取发送超时时间(毫秒)
     */
    int64_t getSendTimeout();

    /**
     * @brief 设置发送超时时间(毫秒)
     */
    void setSendTimeout(int64_t timeoutMillionSeconds);

    /**
     * @brief 获取接受超时时间(毫秒)
     */
    int64_t getRecvTimeout();

    /**
     * @brief 设置接受超时时间(毫秒)
     */
    void setRecvTimeout(int64_t timeoutMillionSeconds);

    /**
     * @brief 获取sockopt  @see getsockopt
     */
    bool getOption(int level, int option, void* result, socklen_t* len);

    /**
     * @brief 获取sockopt模板 @see getsockopt
     */
    template<class T>
    bool getOption(int level, int option, T& result) {
        socklen_t length = sizeof(T);
        return getOption(level, option, &result, &length);
    }

    /**
     * @brief 设置sockopt @see setsockopt
     */
    bool setOption(int level, int option, const void* result, socklen_t len);

    /**
     * @brief 设置sockopt模板 @see setsockopt
     */
    template<class T>
    bool setOption(int level, int option, const T& value) {
        return setOption(level, option, &value, sizeof(T));
    }

    /**
     * @brief 接收connect链接
     * @return 成功返回新连接的socket,失败返回nullptr
     * @pre Socket必须 bind , listen  成功
     */
    [[nodiscard]] virtual Socket::UniquePtr accept();

    /**
     * @brief 绑定地址
     * @param[in] addr 地址
     * @return 是否绑定成功
     */
    virtual bool bind(const Address* addr);

    /**
     * @brief 连接地址
     * @param[in] addr 目标地址
     * @param[in] timeoutMs 超时时间(毫秒)
     */
    virtual bool connect(const Address* addr);

    /**
     * @brief 监听socket
     * @param[in] backlog 未完成连接队列的最大长度默认为SOMAXCONN(128)
     * @result 返回监听是否成功
     * @pre 必须先 bind 成功
     */
    virtual bool listen(int backlog = SOMAXCONN); //#define SOMAXCONN	128

    /**
     * @brief 关闭socket
     */
    virtual bool close();

    /**
     * @brief 发送数据
     * @param[in] buffer 待发送数据的内存
     * @param[in] length 待发送数据的长度
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual int send(const void* buffer, size_t length, int flags = 0);

    /**
     * @brief 发送数据
     * @param[in] buffers 待发送数据的内存(iovec数组)
     * @param[in] length 待发送数据的长度(iovec长度)
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual int send(const iovec* buffers, size_t length, int flags = 0);

    /**
     * @brief 发送数据
     * @param[in] buffer 待发送数据的内存
     * @param[in] length 待发送数据的长度
     * @param[in] target 发送的目标地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual int sendTo(const void* buffer, size_t length, 
        const Address* target, int flags = 0);

    /**
     * @brief 发送数据
     * @param[in] buffers 待发送数据的内存(iovec数组)
     * @param[in] length 待发送数据的长度(iovec长度)
     * @param[in] target 发送的目标地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual int sendTo(const iovec* buffers, size_t length, 
        const Address* target, int flags = 0);

    /**
     * @brief 接受数据
     * @param[out] buffer 接收数据的内存
     * @param[in] length 接收数据的内存大小
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual int recv(void* buffer, size_t length, int flags = 0);

    /**
     * @brief 接受数据
     * @param[out] buffers 接收数据的内存(iovec数组)
     * @param[in] length 接收数据的内存大小(iovec数组长度)
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual int recv(iovec* buffers, size_t length, int flags = 0);

    /**
     * @brief 接受数据
     * @param[out] buffer 接收数据的内存
     * @param[in] length 接收数据的内存大小
     * @param[out] src 发送端地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual int recvFrom(void* buffer, size_t length, 
        Address* src, int flags = 0);

    /**
     * @brief 接受数据
     * @param[out] buffers 接收数据的内存(iovec数组)
     * @param[in] length 接收数据的内存大小(iovec数组长度)
     * @param[out] src 发送端地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual int recvFrom(iovec* buffers, size_t length, Address* src, int flags = 0);

    /**
     * @brief 获取远端地址
     */
    Address* getRemoteAddress();

    /**
     * @brief 获取本地地址
     */
    Address* getLocalAddress();

    const SocketAttribute& getAttribute() const { return sockAttr_; }

    /**
     * @brief 返回是否连接
     */
    bool isConnect() const { return isConnect_; }

    /**
     * @brief 是否有效
     */
    bool isValid() const { return sockFd_ != -1; }

    /**
     * @brief 返回Socket错误
     */
    int getError();

    /**
     * @brief 输出信息到流中
     */
    virtual std::ostream& dump(std::ostream& os) const;

    /**
     * @brief 返回可读性字符串
     */
    virtual String toString() const;

    /**
     * @brief 返回socket文件描述符
     */
    int getSocketFd() const { return sockFd_; }

protected:
    /**
     * @brief 初始化socket
     */
    void initSock();

    /**
     * @brief 创建socket
     */
    void newSock();

    /**
     * @brief 初始化sock
     */
    virtual bool init(int sock);

protected:
    Address::UniquePtr localAddress_;  ///< 本地地址
    Address::UniquePtr remoteAddress_; ///< 远端地址
    SocketAttribute sockAttr_;         ///< 属性
    int sockFd_;                       ///< socket文件描述符
    bool isConnect_;                   ///< 是否连接
};

class SecureSocket : public Socket {
public:
    typedef std::shared_ptr<SecureSocket> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<SecureSocket> UniquePtr; ///< 智能指针定义

    static SecureSocket::UniquePtr CreateTcp(Address* address);
    static SecureSocket::UniquePtr CreateTcpSocket(Address* address);
    static SecureSocket::UniquePtr CreateTcpSocket6(Address* address);

    /**
     * @brief 构造函数
     * @param[in] family ip地址类型
     * @param[in] type 传输协议类型(流/报文)
     * @param protocol 传输协议
     */
    SecureSocket(int family, int type, int protocol = 0);

    SecureSocket(const SocketAttribute& sockAttr);

    [[nodiscard]] Socket::UniquePtr accept() override;
    bool bind(const Address* addr) override;
    bool connect(const Address* addr) override;
    bool listen(int backlog = SOMAXCONN) override;
    bool close() override;
    int send(const void* buffer, size_t length, int flags = 0) override;
    int send(const iovec* buffers, size_t length, int flags = 0) override;
    int sendTo(const void* buffer, size_t length, const Address* to, int flags = 0) override;
    int sendTo(const iovec* buffers, size_t length, const Address* to, int flags = 0) override;
    int recv(void* buffer, size_t length, int flags = 0) override;
    int recv(iovec* buffers, size_t length, int flags = 0) override;
    int recvFrom(void* buffer, size_t length, Address* from, int flags = 0) override;
    int recvFrom(iovec* buffers, size_t length, Address* from, int flags = 0) override;

    /**
     * @brief 加载证书
     * @param[in] cert_file 证书文件
     * @param[in] key_file 秘钥文件
     * @return
     *      @retval true 加载成功
     *      @retval false 加载失败
     */
    bool loadCertificates(StringArg certFile, StringArg keyFile);

    /**
     * @brief 输出信息到流中
     */
    virtual std::ostream& dump(std::ostream& os) const override;

protected:
    bool init(int sock) override;

private:
    struct SslDeleter {
        void operator()(SSL* ssl) {
            SSL_free(ssl);
        }
    };
    struct SslCtxDeleter {
        void operator()(SSL_CTX* sslCtx) {
            SSL_CTX_free(sslCtx);
        }
    };

private:
    std::shared_ptr<SSL_CTX> sslCtx_;       ///< ssl 上下文
    std::unique_ptr<SSL, SslDeleter> ssl_;  ///< ssl
};

/**
 * @brief 流式输出socket
 * @param[in, out] os 输出流
 * @param[in] sock Socket类
 */
std::ostream& operator<<(std::ostream& os, const Socket& sock);

} // namespace net
} // namespace nemo