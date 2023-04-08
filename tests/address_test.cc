#include "log/log.h"
#include "net/address.h"

#include <iterator>
#include <vector>

static nemo::Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");

void test();
void test_iface();
void test_ipv4();
void test_ipv6();

int main(int argc, char* argv[]) {
    test();
    //test_iface();
    //test_ipv4();
    //test_ipv6();

    return 0;
}

void test() {
    NEMO_LOG_DEBUG(rootLogger) << "begin";
    std::vector<nemo::net::Address::UniquePtr> addrs;
    nemo::net::Address::Lookup("www.baidu.com", std::back_insert_iterator(addrs), nemo::net::SocketAttribute(AF_INET, SOCK_STREAM, 0));
    NEMO_LOG_DEBUG(rootLogger) << "end";
    if(addrs.empty()) {
        NEMO_LOG_ERROR(rootLogger) << "lookup fail";
        return;
    }

    for(size_t i = 0; i < addrs.size(); ++i) {
        NEMO_LOG_DEBUG(rootLogger) << i << " - " << addrs[i]->toString();
    }
}

void test_iface() {
    std::multimap<std::string, std::pair<nemo::net::Address::UniquePtr, uint32_t>> results = 
        nemo::net::Address::GetInterfaceAddresses();
    if(results.empty()) {
        NEMO_LOG_ERROR(rootLogger) << "GetInterfaceAddresses fail";
        return;
    }

    for(auto& i: results) {
        NEMO_LOG_DEBUG(rootLogger) << i.first << " - " << i.second.first->toString() << " - "
            << i.second.second;
    }
}

void test_ipv4() {
    nemo::net::Ipv4Address::UniquePtr ipv4 = nemo::net::Ipv4Address::Create("127.0.0.1", 80);
    if(ipv4) {
        NEMO_LOG_INFO(rootLogger) << ipv4->toString();
    } else {
        NEMO_LOG_ERROR(rootLogger) << "create ipv4 failed";
    }
}

void test_ipv6() {
    nemo::net::Ipv6Address::UniquePtr ipv6 = nemo::net::Ipv6Address::Create("fe80::278e:ce59:caae:fa83", 443);
    if(ipv6) {
        NEMO_LOG_INFO(rootLogger) << ipv6->toString();
    } else {
        NEMO_LOG_ERROR(rootLogger) << "create ipv6 failed";
    }
}