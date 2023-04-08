#include "net/address.h"

#include <netdb.h>

#include "log/log.h"
#include "util/util.h"
#include "system/endian.h"

namespace nemo {
namespace net {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

template<typename T>
static T CreateMask(uint32_t bits) {
    return (1 << (sizeof(T) * 8 - bits)) - 1;
}

struct addrinfo* GetAddrInfo(StringArg host, const SocketAttribute& attr) {
    struct addrinfo hints;
    MemoryZero(&hints, sizeof hints);
    hints.ai_family = attr.family;
    hints.ai_socktype = attr.type;
    hints.ai_protocol = attr.protocol;

    String node;                    //主机名
    const char* service = nullptr; //端口号

    //检查ipv6 address service
    //http://[fe80::70c9:e677:9e95:d109]:8080/
    //http://183.232.231.174:80
    if(!host.empty() && host[0] == '[') { //证明是一个ipv6
        const char* endipv6 = (const char*)::memchr(host.data() + 1, ']', host.size() - 1);
        if(endipv6) {
            if(static_cast<decltype(host.size())>(endipv6 - host.data() + 1) < host.size() &&
                *(endipv6 + 1) == ':') {
                service = endipv6 + 2;
            }
            node = host.substr(1, endipv6 - host.data() - 1);
        } else {
            return nullptr;
        }
    }

    //检查 node serivce
    if(node.empty()) { //证明是一个ipv4
        service = (const char*)::memchr(host.data(), ':', host.size());
        if(service) {
            if(!::memchr(service + 1, ':', host.data() + host.size() - service - 1)) {
                node = host.substr(0, service - host.data());
                ++service;
            }
        }
    }

    if(node.empty()) {
        node = host;
    }

    addrinfo* result;
    int error = ::getaddrinfo(node.data(), service, &hints, &result);
    if(error) {
        NEMO_LOG_DEBUG(systemLogger) << "getAddrInfo(" << host << ", "
            << attr.family << ", " << attr.type << ") returned: " << error << " errstr="
            << ::gai_strerror(error);
        return nullptr;
    }

    return result;
}

Address::UniquePtr Address::Create(const sockaddr* addr, socklen_t addrlen) {
    if(!addr) {
        return nullptr;
    }

    switch(addr->sa_family) {
        case AF_INET:
            return std::make_unique<Ipv4Address>(*(const sockaddr_in*)addr);
            break;
        case AF_INET6:
            return std::make_unique<Ipv6Address>(*(const sockaddr_in6*)addr);
            break;
        case AF_UNIX:
            return std::make_unique<UnixAddress>(*(const sockaddr_un*)addr);
            break;
        default:
            return std::make_unique<UnknownAddress>(*addr);
            break;
    }
    return nullptr;
}

Address::UniquePtr Address::LookupAny(StringArg host, const SocketAttribute& attr) {
    addrinfo* info = GetAddrInfo(host, attr);
    if (nullptr == info) {
        return nullptr;
    }
    addrinfo* next = info;
    Address::UniquePtr result = Create(next->ai_addr, (socklen_t)next->ai_addrlen);
    ::freeaddrinfo(info);

    return result;
}

std::unique_ptr<IpAddress> Address::LookupAnyIPAddress(StringArg host, 
            const SocketAttribute& attr) {
    struct addrinfo* info = GetAddrInfo(host, attr);
    if (nullptr == info) {
        return nullptr;
    }

    struct addrinfo* next = info;
    std::unique_ptr<IpAddress> result;
    while (next) {
        switch (next->ai_addr->sa_family) {
            case AF_INET:
                result = std::make_unique<Ipv4Address>(*(const sockaddr_in*)next->ai_addr);
                goto out;
                break;
            case AF_INET6:
                result = std::make_unique<Ipv6Address>(*(const sockaddr_in6*)next->ai_addr);
                goto out;
                break;
            default:
                break;
        }
        next = next->ai_next;
    }

out:
    ::freeaddrinfo(info);

    return result;
}

std::multimap<String, std::pair<Address::UniquePtr, uint32_t>>
Address::GetInterfaceAddresses(int family) {
    struct ifaddrs *next = nullptr;
    struct ifaddrs *result = nullptr;
    std::multimap<String, std::pair<Address::UniquePtr, uint32_t>> addresses;
    if(::getifaddrs(&result) != 0) {
        NEMO_LOG_ERROR(systemLogger) << "Address::GetInterfaceAddresses getifaddrs "
            " err=" << errno << " errstr=" << strerror(errno);
    } else {
        try {
            for(next = result; next; next = next->ifa_next) {
                Address::UniquePtr addr;
                uint32_t prefixLen = (uint32_t)(-1);
                if(family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                    continue;
                }
                switch(next->ifa_addr->sa_family) {
                    case AF_INET: {
                            addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                            uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                            prefixLen = std::popcount(netmask);
                        }
                        break;
                    case AF_INET6: {
                            addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                            in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                            prefixLen = 0;
                            for(int i = 0; i < 16; ++i) {
                                prefixLen += std::popcount(netmask.s6_addr[i]);
                            }
                        }
                        break;
                    default:
                        break;
                }

                if(addr) {
                    addresses.emplace(next->ifa_name, std::make_pair(std::move(addr), prefixLen));
                }
            }
        } catch (...) {
            NEMO_LOG_ERROR(systemLogger) << "Address::GetInterfaceAddresses exception";
            freeifaddrs(result);
            addresses.clear();
        }
        freeifaddrs(result);
    }

    return addresses;
}

int Address::getFamily() const {
    return getAddr()->sa_family;
}

String Address::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

bool Address::operator<(const Address& other) const {
    socklen_t minLen = std::min(getAddrLen(), other.getAddrLen());
    int result = ::memcmp(getAddr(), other.getAddr(), minLen);
    if(result < 0) {
        return true;
    } else if(result > 0) {
        return false;
    } else if(getAddrLen() < other.getAddrLen()) {
        return true;
    }
    return false;
}

bool Address::operator==(const Address& other) const {
    return getAddrLen() == other.getAddrLen() && 
           memcmp(getAddr(), other.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& other) const {
    return !(*this == other);
}

IpAddress::UniquePtr IpAddress::Create(const char* address, uint16_t port) {
    addrinfo hints, *results;
    MemoryZero(&hints, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;	//强制为数字类型的网络地址
    hints.ai_family = AF_UNSPEC;		//getaddrinfo应该返回socket地址

    int error = ::getaddrinfo(address, NULL, &hints, &results);
    IpAddress::UniquePtr ipAddress;
    if(error) {
        NEMO_LOG_ERROR(systemLogger) << "IPAddress::Create(" << address
            << ", " << port << ") error=" << error
            << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }

    try {
        Address::UniquePtr addr;
        addr = Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen);
        ipAddress.reset(dynamic_cast<IpAddress*>(addr.get()));
        addr.release();
        if(ipAddress) {
            ipAddress->setPort(port);
        }
        ::freeaddrinfo(results);
    } catch (...) {
        ::freeaddrinfo(results);
    }

    return ipAddress;
}

Ipv4Address::UniquePtr Ipv4Address::Create(const char* address, uint16_t port) {
    Ipv4Address::UniquePtr ipv4Address = std::make_unique<Ipv4Address>();
    ipv4Address->addr_.sin_port = ByteSwapOnLittleEndian(port);
    int result = ::inet_pton(AF_INET, address, &ipv4Address->addr_.sin_addr);
    if(result <= 0) {
        NEMO_LOG_ERROR(systemLogger) << "Ipv4Address::Create(" << address << ", "
                << port << ") rt=" << result << " errno=" << errno
                << " errstr=" << strerror(errno);
        return nullptr;
    }
    return ipv4Address;
}

Ipv4Address::Ipv4Address(const sockaddr_in& address) :
    addr_(address) {
}

Ipv4Address::Ipv4Address(uint32_t address, uint16_t port) {
    MemoryZero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = ByteSwapOnLittleEndian(port);
    addr_.sin_addr.s_addr = ByteSwapOnLittleEndian(address);
}

const sockaddr* Ipv4Address::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&addr_);
}

sockaddr* Ipv4Address::getAddr() {
    return reinterpret_cast<sockaddr*>(&addr_);
}

socklen_t Ipv4Address::getAddrLen() const {
    return sizeof(addr_);
}

std::ostream& Ipv4Address::dump(std::ostream& os) const {
    uint32_t addr = ByteSwapOnLittleEndian(addr_.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "."
       << ((addr >> 16) & 0xff) << "."
       << ((addr >> 8) & 0xff) << "."
       << (addr & 0xff);
    os << ":" << ByteSwapOnLittleEndian(addr_.sin_port);
    return os;
}

Ipv4Address::UniquePtr Ipv4Address::broadcastAddress(uint32_t prefixLen) {
    if(prefixLen > 32) {
        return nullptr;
    }

    sockaddr_in baddr(addr_);
    baddr.sin_addr.s_addr |= ByteSwapOnLittleEndian(
            CreateMask<uint32_t>(prefixLen));
    return std::make_unique<Ipv4Address>(baddr);
}

IpAddress::UniquePtr Ipv4Address::networkAddress(uint32_t prefixLen) {
    if(prefixLen > 32) {
        return nullptr;
    }

    sockaddr_in baddr(addr_);
    baddr.sin_addr.s_addr &= ByteSwapOnLittleEndian(
            CreateMask<uint32_t>(prefixLen));
    return std::make_unique<Ipv4Address>(baddr);
}

Ipv4Address::UniquePtr Ipv4Address::subnetMask(uint32_t prefixLen) {
    sockaddr_in subnet;
    MemoryZero(&subnet, sizeof(subnet));
    subnet.sin_family = AF_INET;
    subnet.sin_addr.s_addr = ~ByteSwapOnLittleEndian(CreateMask<uint32_t>(prefixLen));
    return std::make_unique<Ipv4Address>(subnet);
}

uint16_t Ipv4Address::getPort() const {
    return ByteSwapOnLittleEndian(addr_.sin_port);
}

void Ipv4Address::setPort(uint16_t port) {
    addr_.sin_port = ByteSwapOnLittleEndian(port);
}

Ipv6Address::UniquePtr Ipv6Address::Create(const char* address, uint16_t port) {
    Ipv6Address::UniquePtr ipv6Address = std::make_unique<Ipv6Address>();
    ipv6Address->addr_.sin6_port = ByteSwapOnLittleEndian(port);
    int result = ::inet_pton(AF_INET6, address, &ipv6Address->addr_.sin6_addr); //把ip地址转化为用于网络传输的二进制数值
    if(result <= 0) {
        NEMO_LOG_ERROR(systemLogger) << "Ipv6Address::Create(" << address << ", "
                << port << ") rt=" << result << " errno=" << errno
                << " errstr=" << strerror(errno);
        return nullptr;
    }
    return ipv6Address;
}

Ipv6Address::Ipv6Address() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
}

Ipv6Address::Ipv6Address(const sockaddr_in6& address) {
    addr_ = address;
}

Ipv6Address::Ipv6Address(const uint8_t address[16], uint16_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
    addr_.sin6_port = ByteSwapOnLittleEndian(port);
    memcpy(&addr_.sin6_addr.s6_addr, address, 16);
}

const sockaddr* Ipv6Address::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&addr_);
}

sockaddr* Ipv6Address::getAddr() {
    return reinterpret_cast<sockaddr*>(&addr_);
}

socklen_t Ipv6Address::getAddrLen() const {
    return sizeof(addr_);
}

std::ostream& Ipv6Address::dump(std::ostream& os) const {
    os << "[";
    uint16_t* addr = (uint16_t*)addr_.sin6_addr.s6_addr;
    bool usedZeros = false;
    for(size_t i = 0; i < 8; ++i) {
        if(addr[i] == 0 && !usedZeros) {
            continue;
        }
        if(i && addr[i - 1] == 0 && !usedZeros) {
            os << ":";
            usedZeros = true;
        }
        if(i) {
            os << ":";
        }
        os << std::hex << static_cast<int>(ByteSwapOnLittleEndian(addr[i])) << std::dec;
    }

    if(!usedZeros && addr[7] == 0) {
        os << "::";
    }

    os << "]:" << ByteSwapOnLittleEndian(addr_.sin6_port);
    return os;
}

IpAddress::UniquePtr Ipv6Address::networkAddress(uint32_t prefixLen) {
    sockaddr_in6 baddr(addr_);
    baddr.sin6_addr.s6_addr[prefixLen / 8] &=
        CreateMask<uint8_t>(prefixLen % 8);
    for(int i = prefixLen / 8 + 1; i < 16; ++i) {
        baddr.sin6_addr.s6_addr[i] = 0x00;
    }
    return std::make_unique<Ipv6Address>(baddr);
}

uint16_t Ipv6Address::getPort() const {
    return ByteSwapOnLittleEndian(addr_.sin6_port);
}   

void Ipv6Address::setPort(uint16_t port) {
    addr_.sin6_port = ByteSwapOnLittleEndian(port);
}

constexpr static size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

UnixAddress::UnixAddress() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX;
    length_ = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const sockaddr_un& address) {
    addr_ = address;
}

UnixAddress::UnixAddress(const std::string& path) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX;
    length_ = path.size() + 1;

    if(!path.empty() && path[0] == '\0') {
        --length_;
    }

    if(length_ > sizeof(addr_.sun_path)) {
        throw std::logic_error("path too long");
    }
    ::memcpy(addr_.sun_path, path.c_str(), length_);
    length_ += offsetof(sockaddr_un, sun_path);
}

const sockaddr* UnixAddress::getAddr() const {
    return (sockaddr*)&addr_;
}

sockaddr* UnixAddress::getAddr() {
    return (sockaddr*)&addr_;
}

socklen_t UnixAddress::getAddrLen() const {
    return length_;
}

void UnixAddress::setAddrLen(uint32_t value) {
    length_ = value;
}

std::string UnixAddress::getPath() const {
    std::stringstream ss;
    if(length_ > offsetof(sockaddr_un, sun_path) && 
       addr_.sun_path[0] == '\0') {
        ss << "\\0" << std::string(addr_.sun_path + 1,
                length_ - offsetof(sockaddr_un, sun_path) - 1);
    } else {
        ss << addr_.sun_path;
    }
    return ss.str();
}

std::ostream& UnixAddress::dump(std::ostream& os) const {
    if(length_ > offsetof(sockaddr_un, sun_path) && 
       addr_.sun_path[0] == '\0') {
        return os << "\\0" << std::string(addr_.sun_path + 1,
                length_ - offsetof(sockaddr_un, sun_path) - 1);
    }
    return os << addr_.sun_path;
}

UnknownAddress::UnknownAddress(int family) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr) {
    addr_ = addr;
}

const sockaddr* UnknownAddress::getAddr() const {
    return (sockaddr*)&addr_;
}

sockaddr* UnknownAddress::getAddr() {
    return (sockaddr*)&addr_;
}

socklen_t UnknownAddress::getAddrLen() const {
    return sizeof(addr_);
}

std::ostream& UnknownAddress::dump(std::ostream& os) const {
    os << "[UnknownAddress family=" << addr_.sa_family << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Address& addr) {
    return addr.dump(os);
}

} // namespace net
} // namespace nemo