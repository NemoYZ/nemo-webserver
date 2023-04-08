#include "net/http/http_session.h"

#include "net/http/http_parser.h"
#include "container/buffer.h"

namespace nemo {
namespace net {
namespace http {

HttpSession::HttpSession(Socket* sock) :
    sockStream_(std::make_unique<io::SocketStream>(sock)) {
}

HttpSession::HttpSession(Socket::UniquePtr&& sock) :
    sockStream_(std::make_unique<io::SocketStream>(std::move(sock))) {
}

HttpRequest::UniquePtr HttpSession::recvRequest() {
    HttpRequestParser::UniquePtr parser = std::make_unique<HttpRequestParser>();
    const size_t buffSize = HttpRequestParser::GetHttpRequestBufferSize();
    Buffer buffer(buffSize, true);
    char* data = buffer.data();
    off_t offset = 0;

    do {
        int len = sockStream_->read(data + offset, buffSize - offset);
        if(len <= 0) {
            sockStream_->close();
            return nullptr;
        }
        len += offset; //因为上一次可能没解析完，所以要加上上一次的offset
        size_t nparse = parser->execute(data, len);
        if(parser->hasError()) {
            sockStream_->close();
            return nullptr;
        }
        offset = len - nparse; //剩余未解析的
        if(offset == (int)buffSize) { //根本就没有解析
            sockStream_->close();
            return nullptr;
        }
        if(parser->isFinished()) {
            break;
        }
    } while(true);
    
    int64_t bodyLen = parser->getContentLength();
    if(bodyLen > 0) {
        String body;
        body.resize(bodyLen);

        int64_t len = std::min(static_cast<int64_t>(offset), bodyLen);
        //把data里剩余的数据拷贝到body中
        ::memcpy(body.data(), data, len);
        bodyLen -= offset;
        if(bodyLen > 0) { //证明在read的时候并没有读取到所有数据
            if(sockStream_->readFixSize(&body[len], bodyLen) <= 0) { //把剩余的数据读取出来
                sockStream_->close();
                return nullptr;
            }
        }
        parser->getRequest()->setBody(std::move(body));
    }

    parser->getRequest()->init();
    HttpRequest::UniquePtr request = std::move(parser->request_);
    return request;
}

int HttpSession::sendResponse(const HttpResponse* response) {
    String msg = response->toString();
    return sockStream_->writeFixSize(msg.data(), msg.size());
}

} // namespace http
} // namespace net
} // namespace nemo