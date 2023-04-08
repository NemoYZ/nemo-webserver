#include "net/http/http_parser.h"

#include <iomanip>

#include "net/address.h"
#include "net/socket.h"
#include "log/log.h"
#include "common/types.h"
#include "container/buffer.h"

using namespace nemo;

static nemo::Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");

void TestRequest(nemo::StringArg httpMsg);
void TestResponse(nemo::StringArg httpMsg);
void Test(StringArg uriStr);

int main(int argc, char** argv) {
    TestRequest("");
    //TestResponse("");
    //Test("www.baidu.com");

    return 0;
}

void TestRequest(StringArg httpMsg) {
    String msg(httpMsg.data(), httpMsg.size());
    if (msg.empty()) {
        msg = "POST / HTTP/1.1\r\n"
              "Host: www.baidu.com\r\n"
              "Content-Length: 10\r\n"
              "\r\n"
              "0123456789";
    }

    net::http::HttpRequestParser parser;
    
    size_t offset = parser.execute(msg.data(), msg.size());
    NEMO_LOG_DEBUG(rootLogger) << std::boolalpha
                        << "execute result = " << offset
                        << " has error = " << parser.hasError()
                        << " is finished = " << parser.isFinished()
                        << " total = " << msg.size()
                        << " content-length = " << parser.getContentLength();
    NEMO_LOG_DEBUG(rootLogger) << "offset=" << offset;
    msg.resize(msg.size() - offset);
    parser.getRequest()->setBody(msg);
    NEMO_LOG_DEBUG(rootLogger) << "request = \n" << parser.getRequest()->toString();
    NEMO_LOG_DEBUG(rootLogger) << "msg=" << msg;
}

void TestResponse(StringArg httpMsg) {
    String msg(httpMsg.data(), httpMsg.size());
    if (msg.empty()) {
        msg = "HTTP/1.1 200 OK\r\n"
        "Date: Tue, 04 Jun 2019 15:43:56 GMT\r\n"
        "Server: Apache\r\n"
        "Last-Modified: Tue, 12 Jan 2010 13:48:00 GMT\r\n"
        "ETag: \"51-47cf7e6ee8400\"\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 81\r\n"
        "Cache-Control: max-age=86400\r\n"
        "Expires: Wed, 05 Jun 2019 15:43:56 GMT\r\n"
        "Connection: Close\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html>\r\n"
        "<meta http-equiv=\"refresh\" content=\"0;url=http://www.baidu.com/\">\r\n"
        "</html>\r\n";
    }

    net::http::HttpResponseParser parser;
    size_t offset = parser.execute(msg.data(), msg.size(), false);
    NEMO_LOG_DEBUG(rootLogger) << std::boolalpha
                    << "execute result = " << offset
                    << " has error = " << parser.hasError()
                    << " is finished = " << parser.isFinished()
                    << " total = " << msg.size()
                    << " content-length = " << parser.getContentLength();
    NEMO_LOG_DEBUG(rootLogger) << "offset=" << offset;
    NEMO_LOG_DEBUG(rootLogger) << "response = \n" << parser.getResponse()->toString();
}

void Test(StringArg uriStr) {
    net::http::HttpRequest::UniquePtr request = std::make_unique<net::http::HttpRequest>();
    net::IpAddress::UniquePtr address = net::Address::LookupAnyIPAddress(uriStr);
    if (!address) {
        NEMO_LOG_ERROR(rootLogger) << "查询ip地址失败";
        return;
    } else {
        address->setPort(80);
        NEMO_LOG_DEBUG(rootLogger) << "查询到的ip地址为: " << address->toString();
    }
    net::Socket::UniquePtr sock = net::Socket::CreateTcpSocket();
    if (!sock->connect(address.get())) {
        NEMO_LOG_ERROR(rootLogger) << "连接失败";
        return;
    } else {
        NEMO_LOG_DEBUG(rootLogger) << "连接成功";
    }
    String sendMsg = request->toString();
    NEMO_LOG_INFO(rootLogger) << "sendMsg = " << sendMsg;
    if (sock->send(sendMsg.data(), sendMsg.size()) <= 0) {
        NEMO_LOG_ERROR(rootLogger) << "send message failed" 
            << " errno=" << errno << " errstr=" << strerror(errno);
        return; 
    }
    Buffer buff;
    buff.resize(4096);
    if (sock->recv(buff.data(), buff.size()) <= 0) {
        NEMO_LOG_ERROR(rootLogger) << "recv message failed" 
            << " errno=" << errno << " errstr=" << strerror(errno);
        return; 
    }
    NEMO_LOG_DEBUG(rootLogger) << "buf=" << buff.data();

    TestResponse(StringArg(buff.data(), buff.size()));
}