#pragma once

namespace nemo {
namespace net {

struct SocketAttribute {
    enum Type {
        Tcp = SOCK_STREAM,  ///< Tcp类型
        Udp = SOCK_DGRAM    ///< Udp类型
    };

    enum Family {
        Ipv4 = AF_INET,     ///< IPv4 socket
        Ipv6 = AF_INET6,    ///< IPv6 socket
        Unix = AF_UNIX,     ///< Unix socket
    };

    SocketAttribute() = default;
    SocketAttribute(int sockFamily, int sockType, int sockProtocol) :
        family(sockFamily),
        type(sockType),
        protocol(sockProtocol) {
    }
    bool valid() const { return family != -1; }

    int family{-1};
    int type{-1};
    int protocol{-1};
};

} // namespace net
} // namespace nemo