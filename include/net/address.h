#pragma once

#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <map>
#include <memory>
#include <iterator>

#include "net/socket_attribute.h"
#include "common/types.h"

namespace nemo {
namespace net {

class IpAddress;

struct addrinfo* GetAddrInfo(StringArg host, const SocketAttribute& attr);

/**
 * @brief 地址的基类
 */
class Address {
public:
    typedef std::shared_ptr<Address> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<Address> UniquePtr; ///< 智能指针定义

    /**
     * @brief 通过sockaddr指针创建Address
     * @param[in] addr sockaddr指针
     * @param[in] addrlen sockaddr的长度
     * @return 返回和sockaddr相匹配的Address,失败返回nullptr
     */
    static Address::UniquePtr Create(const sockaddr* addr, 
        socklen_t addrlen);

    /**
     * @brief 通过host地址返回对应条件的所有Address
     * @param[out] result 保存满足条件的Address
     * @param[in] host 域名,服务器名等.举例: www.baidu.top[:80] (方括号为可选内容)
     * @param[in] attr socket的属性(类型, tcp/udp, 协议)
     * @return 最后一个地址在容器中的位置
     */
    template<typename Iter>
    static Iter Lookup(StringArg host,
        Iter dest,
        const SocketAttribute& attr);

    /**
     * @brief 通过host地址返回对应条件的任意Address
     * @param[in] host 域名,服务器名等.举例: www.baidu.top[:80] (方括号为可选内容)
     * @param[in] attr socket的属性
     * @return 返回满足条件的任意Address,失败返回nullptr
     */
    static Address::UniquePtr LookupAny(StringArg host,
            const SocketAttribute& attr = SocketAttribute(AF_INET, SOCK_STREAM, 0));
    
    /**
     * @brief 通过host地址返回对应条件的任意IPAddress
     * @param[in] host 域名,服务器名等.举例: www.com.top[:80] (方括号为可选内容)
     * @param[in] attr socket的属性
     * @return 有满足条件的任意IPAddress, 返回true, 否则返回false
     */
    static std::unique_ptr<IpAddress> LookupAnyIPAddress(StringArg host, 
            const SocketAttribute& attr = SocketAttribute(AF_INET, SOCK_STREAM, 0));
    
    /**
     * @brief 返回本机所有网卡的<网卡名, 地址, 子网掩码位数>
     * @param[out] result 保存本机所有地址
     * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
     * @return 是否获取成功
     */
    static std::multimap<String, std::pair<Address::UniquePtr, uint32_t>> 
    GetInterfaceAddresses(int family = AF_INET);

    /**
     * @brief 获取指定网卡的地址和子网掩码位数
     * @param[in] iface 网卡名称
     * @param[in] dest 输出目的地
     * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
     * @return 是否获取成功
     */
    template<typename Iter>
    static Iter GetInterfaceAddresses(StringArg iface, 
        Iter dest, int family = AF_INET);

    /**
     * @brief 析构函数
     */
    virtual ~Address() = default;

    /**
     * @brief 返回协议簇
     */
    int getFamily() const;

    /**
     * @brief 返回sockaddr指针,只读
     */
    virtual const sockaddr* getAddr() const = 0;

    /**
     * @brief 返回sockaddr指针,读写
     */
    virtual sockaddr* getAddr() = 0;

    /**
     * @brief 返回sockaddr的长度
     */
    virtual socklen_t getAddrLen() const = 0;

    /**
     * @brief 可读性输出地址
     */
    virtual std::ostream& dump(std::ostream& os) const = 0;

    /**
     * @brief 返回可读性字符串
     */
    String toString() const;

    /**
     * @brief 小于号比较函数
     */
    bool operator<(const Address& other) const;

    /**
     * @brief 等于函数
     */
    bool operator==(const Address& other) const;

    /**
     * @brief 不等于函数
     */
    bool operator!=(const Address& other) const;
};

/**
 * @brief IP地址的基类
 */
class IpAddress : public Address {
public:
    typedef std::shared_ptr<IpAddress> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<IpAddress> UniquePtr; ///< 智能指针定义

    /**
     * @brief 通过域名,IP,服务器名创建IPAddress
     * @param[in] address 域名,IP,服务器名等.举例: www.sylar.top
     * @param[in] port 端口号
     * @return 调用成功返回IPAddress,失败返回nullptr
     */
    static IpAddress::UniquePtr Create(const char* address, uint16_t port = 0);

    /**
     * @brief 获取该地址的网段
     * @param[in] prefixLen 子网掩码位数
     * @return 调用成功返回IPAddress,失败返回nullptr
     */
    virtual IpAddress::UniquePtr networkAddress(uint32_t prefixLen) = 0;

    /**
     * @brief 返回端口号
     */
    virtual uint16_t getPort() const = 0;

    /**
     * @brief 设置端口号
     */
    virtual void setPort(uint16_t port) = 0;
};

/**
 * @brief IPv4地址
 */
class Ipv4Address : public IpAddress {
public:
    typedef std::shared_ptr<Ipv4Address> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<Ipv4Address> UniquePtr; ///< 智能指针定义

    /**
     * @brief 使用点分十进制地址创建IPv4Address
     * @param[in] address 点分十进制地址,如:192.168.1.1
     * @param[in] port 端口号
     * @return 返回IPv4Address,失败返回nullptr
     */
    static Ipv4Address::UniquePtr Create(const char* address, 
        uint16_t port = 0);

    /**
     * @brief 通过sockaddr_in构造IPv4Address
     * @param[in] address sockaddr_in结构体
     */
    Ipv4Address(const sockaddr_in& address);

    /**
     * @brief 通过二进制地址构造IPv4Address
     * @param[in] address 二进制地址address
     * @param[in] port 端口号
     */
    Ipv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& dump(std::ostream& os) const override;

    IpAddress::UniquePtr networkAddress(uint32_t prefixLen) override;
    uint16_t getPort() const override;
    void setPort(uint16_t port) override;

    Ipv4Address::UniquePtr broadcastAddress(uint32_t prefixLen);
    Ipv4Address::UniquePtr subnetMask(uint32_t prefixLen);

private:
    sockaddr_in addr_;     ///< ipv4对应的socket结构体
};

/**
 * @brief IPv6地址
 */
class Ipv6Address : public IpAddress {
public:
    typedef std::shared_ptr<Ipv6Address> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<Ipv6Address> UniquePtr; ///< 智能指针定义

    /**
     * @brief 通过IPv6地址字符串构造IPv6Address
     * @param[in] address IPv6地址字符串
     * @param[in] port 端口号
     */
    static Ipv6Address::UniquePtr Create(const char* address, uint16_t port = 0);

    /**
     * @brief 无参构造函数
     */
    Ipv6Address();

    /**
     * @brief 通过IPv6二进制地址构造IPv6Address
     * @param[in] address IPv6二进制地址
     */
    Ipv6Address(const uint8_t address[16], uint16_t port = 0);

    /**
     * @brief 通过IPv6二进制地址构造IPv6Address
     * @param[in] address IPv6二进制地址
     */
    Ipv6Address(const sockaddr_in6& address);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& dump(std::ostream& os) const override;

    IpAddress::UniquePtr networkAddress(uint32_t prefixLen) override;
    uint16_t getPort() const override;
    void setPort(uint16_t port) override;

private:
    sockaddr_in6 addr_;    ///< ipv6对应的socket结构体
};

/**
 * @brief UnixSocket地址
 */
class UnixAddress : public Address {
public:
    typedef std::shared_ptr<UnixAddress> ptr; ///智能指针定义

    /**
     * @brief 无参构造函数
     */
    UnixAddress();

    /**
     * 
     * @brief 通过Unix二进制地址构造UnixAddress
     * @param[in] address Unix二进制地址
     */
    UnixAddress(const sockaddr_un& address);

    /**
     * @brief 通过路径构造UnixAddress
     * @param[in] path UnixSocket路径(长度小于UNIX_PATH_MAX)
     */
    UnixAddress(const std::string& path);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    void setAddrLen(uint32_t value);
    std::string getPath() const;
    std::ostream& dump(std::ostream& os) const override;

private:
    sockaddr_un addr_; ///< unix socket对应的结构体
    socklen_t length_; ///< 结构体长度
};


/**
 * @brief 未知地址
 */
class UnknownAddress : public Address {
public:
    typedef std::shared_ptr<UnknownAddress> ptr;

    /**
     * @brief 构造函数，通过family构造一个地址
     * @param [in] family 协议族
     */
    UnknownAddress(int family);

    /**
     * @brief 构造函数，通过sockaddr构造一个地址
     * @param [in] sockaddr socket
     */
    UnknownAddress(const sockaddr& addr);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& dump(std::ostream& os) const override;

private:
    sockaddr addr_; ///< socket地址
};

/**
 * @brief 流式输出Address
 */
std::ostream& operator<<(std::ostream& os, const Address& addr);

template<typename Iter>
Iter Address::Lookup(StringArg host,
    Iter dest,
    const SocketAttribute& attr) {
    struct addrinfo* info = GetAddrInfo(host, attr);
    if (nullptr == info) {
        return dest;
    }

    struct addrinfo* next = info;
    while (next) {
        Address::UniquePtr tmp = Create(next->ai_addr, (socklen_t)next->ai_addrlen);
        *dest = std::move(tmp);
        ++dest;
        next = next->ai_next;
    }

    ::freeaddrinfo(info);
    return dest;
}

template<typename Iter>
Iter Address::GetInterfaceAddresses(StringArg iface, 
        Iter dest, int family) {    
    if(iface.empty() || iface == "*") {
        Address::UniquePtr tmpAddr;
        if(family == AF_INET || family == AF_UNSPEC) { //默认ipv4
            tmpAddr = std::make_unique<Ipv4Address>();
            
        } else if(family == AF_INET6) {
            tmpAddr = std::make_unique<Ipv6Address>();
        }
        dest = std::make_pair(std::move(tmpAddr), 0U);
        ++dest;
        return dest;
    }

    std::multimap<String, std::pair<Address::UniquePtr, uint32_t>> allAddress = GetInterfaceAddresses(family);
    if (allAddress.empty()) {
        return dest;
    }

    String interface(iface.data(), iface.size());
    auto iters = allAddress.equal_range(interface);
    for ( ; iters.first != iters.second; ++iters.first) {
        dest = std::move(iters.first->second);
        ++dest;
    }

    return dest;
}

} // namespace net    
} // namespace nemo