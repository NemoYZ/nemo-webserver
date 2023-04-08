#include "net/http/http_connection.h"

#include "common/lexical_cast.h"
#include "net/http/http_parser.h"
#include "log/log.h"
#include "container/buffer.h"

namespace nemo {
namespace net {
namespace http {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

HttpResult::HttpResult(ErrorCode errCode,
            HttpResponse::UniquePtr&& res,
            StringArg errMsg) : 
        response(std::move(res)),
        errorMessage(errMsg),
        errorCode(errCode) {
    }

String HttpResult::toString() const {
    std::stringstream ss;
    ss << "[HttpResult errorCode=" << static_cast<int8_t>(errorCode)
       << " errorMsg=" << errorMessage
       << " response=" << (response ? response->toString() : "nullptr")
       << "]";
    return ss.str();
}

HttpConnection::HttpConnection(Socket* sock) :
    sockStream_(std::make_unique<io::SocketStream>(sock)) {
}

HttpConnection::HttpConnection(Socket::UniquePtr&& sock) :
    sockStream_(std::make_unique<io::SocketStream>(std::move(sock))) {
}

HttpConnection::~HttpConnection() {
}

HttpResponse::UniquePtr HttpConnection::recvResponse() {
    HttpResponseParser::UniquePtr parser = std::make_unique<HttpResponseParser>();
    const size_t buffSize = HttpRequestParser::GetHttpRequestBufferSize();
    //uint64_t buffSize = 100;
    Buffer buffer(buffSize + 1, true);
    char* data = buffer.data();
    off_t offset = 0; //下一次读取data的偏移量
    do {
		//len为成功读取的字节数
        int len = sockStream_->read(data + offset, buffSize - offset);
        if(len <= 0) { //读取失败
            sockStream_->close();
            return nullptr;
        }
        //得到总的读取的字节数
        len += offset;
        data[len] = '\0';
        size_t nparse = parser->execute(data, len, false); 
        if(parser->hasError()) { //解析失败
            sockStream_->close();
            return nullptr;
        }
        // 计算未解析字节数
        offset = len - nparse;
        // 没有解析
        if(static_cast<off_t>(buffSize) == offset) {
            sockStream_->close();
            return nullptr;
        }
        if(parser->isFinished()) {
            break;
        }
    } while(true);

    auto& clientParser = parser->getParser();
    String body;
    if(clientParser.chunked) { //分块解析
        //len表示已经读取到的数据总长度，也可以表示还未解析的数据总长度
        off_t len = offset;
        do {
            bool begin = true;
            do {
                if(!begin || len == 0) {
                    int rt = sockStream_->read(data + len, buffSize - len);
                    if(rt <= 0) {
                        sockStream_->close();
                        return nullptr;
                    }
                    len += rt;
                }
                data[len] = '\0';
                size_t nparse = parser->execute(data, len, true);
                if(parser->hasError()) {
                    sockStream_->close();
                    return nullptr;
                }
                len -= nparse;
                if(static_cast<size_t>(len) == buffSize) {
                    sockStream_->close();
                    return nullptr;
                }
                begin = false;
            } while(!parser->isFinished());
            
            NEMO_LOG_DEBUG(systemLogger) << "content_len=" << clientParser.content_len;
            //\r\n占两个字符 context_len + 2 <= len, 说明未解析的数据为body且已经全部读到了
            if(clientParser.content_len + 2 <= len) { 
                body.append(data, clientParser.content_len);
                ::memmove(data, data + clientParser.content_len + 2, 
                    len - clientParser.content_len - 2);
                len -= clientParser.content_len + 2;
            } else {
                body.append(data, len);
                int left = clientParser.content_len - len + 2;
                while(left > 0) {
                    int rt = sockStream_->read(data, left > (int)buffSize ? (int)buffSize : left);
                    if(rt <= 0) {
                        sockStream_->close();
                        return nullptr;
                    }
                    body.append(data, rt);
                    left -= rt;
                }
                body.resize(body.size() - 2);
                len = 0;
            }
        } while(!clientParser.chunks_done);
    } else { //不分块解析
        int64_t bodyLen = static_cast<int64_t>(parser->getContentLength());
        if(bodyLen > 0) {
            body.resize(bodyLen);
            int64_t len = std::min(static_cast<int64_t>(offset), bodyLen);
            ::memcpy(body.data(), data, len);
            bodyLen -= offset;
            if(bodyLen > 0) {
                if(sockStream_->readFixSize(&body[len], bodyLen) <= 0) { //读剩余数据
                    sockStream_->close();
                    return nullptr;
                }
            }
        }
    }

    if(!body.empty()) {
        parser->getResponse()->setBody(std::move(body));
    }
    HttpResponse::UniquePtr response = std::move(parser->response_);
    return response;
}

int HttpConnection::sendRequest(const HttpRequest* request) {
    String data = request->toString();
    return sockStream_->writeFixSize(data.data(), data.size());
}

HttpResult::UniquePtr HttpConnection::DoRequest(HttpMethod method, 
                            StringArg uriStr, 
                            uint64_t timeoutMillionSeconds,
                            const std::map<String, String>& headers, 
                            StringArg body) {
    Uri::UniquePtr uri = Uri::Create(uriStr);
    if(!uri) {
        String errMsg = "invalid url: ";
        errMsg.append(uriStr.data(), uriStr.size());
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::INVALID_URL, 
            nullptr, errMsg);
    }
    return DoRequest(method, uri.get(), timeoutMillionSeconds, headers, body);
}

HttpResult::UniquePtr HttpConnection::DoRequest(HttpMethod method, 
                            const Uri* uri,
                            uint64_t timeoutMillionSeconds, 
                            const std::map<String, String>& headers, 
                            StringArg body) {
    HttpRequest::UniquePtr request = std::make_unique<HttpRequest>();
    request->setPath(uri->getPath());
    request->setQuery(uri->getQuery());
    request->setFragment(uri->getFragment());
    request->setMethod(method);
    bool hasHost = false;
    for(auto& i : headers) {
        if(::strcasecmp(i.first.data(), "connection") == 0) {
            if(::strcasecmp(i.second.data(), "keep-alive") == 0) {
                request->setClose(false);
            }
            continue;
        }

        if(!hasHost && ::strcasecmp(i.first.c_str(), "host") == 0) {
            hasHost = !i.second.empty();
        }

        request->setHeader(i.first, i.second);
    }
    if(!hasHost) {
        request->setHeader("Host", uri->getHost());
    }
    request->setBody(body);
    return DoRequest(request.get(), uri, timeoutMillionSeconds);
}

HttpResult::UniquePtr HttpConnection::DoRequest(HttpRequest* request,
                            const Uri* uri,
                            uint64_t timeoutMillionSeconds) {
    //bool is_ssl = uri->getScheme() == "https";
    IpAddress::UniquePtr addr = uri->createIpAddress();
    if(!addr) {
        String errMsg = "invalid host: " + uri->getHost();
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::INVALID_HOST, 
            nullptr, errMsg);
    }
    Socket::UniquePtr sock = Socket::CreateTcp(addr.get());
    if(!sock->connect(addr.get())) {
        String errMsg = "connect fail: " + addr->toString();
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::CONNECT_FAILED, 
            nullptr, errMsg);
    }
    sock->setRecvTimeout(timeoutMillionSeconds);
    HttpConnection::UniquePtr connection = std::make_unique<HttpConnection>(std::move(sock));
    int ret = connection->sendRequest(request);
    if(0 == ret) {
        String errMsg = "send request closed by peer: " + addr->toString();
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::SEND_CLOSE_BY_PEER, 
            nullptr, errMsg);
    }
    if(ret < 0) {
        String errMsg = "send request socket error errno=" + std::to_string(errno)
                    + " errstr=" + String(strerror(errno));
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::SEND_SOCKET_ERROR, 
            nullptr, errMsg);
    }
    HttpResponse::UniquePtr response = connection->recvResponse();
    if(!response) {
        String errMsg = "recv response timeout: " + addr->toString()
                    + " timeout_ms:" + std::to_string(timeoutMillionSeconds);
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::SEND_SOCKET_ERROR, 
            nullptr, errMsg);
    }

    return std::make_unique<HttpResult>(HttpResult::ErrorCode::OK, 
            nullptr, "ok");
}

HttpConnectionPool::UniquePtr HttpConnectionPool::Create(StringArg uriStr,
        StringArg vhost,
        uint32_t maxSize,
        uint32_t maxAliveTime,
        uint32_t maxRequest) {
    Uri::UniquePtr uri = Uri::Create(uriStr);
    if(!uri) {
        NEMO_LOG_ERROR(systemLogger) << "invalid uri=" << uriStr;
        return nullptr;
    }
    return std::make_unique<HttpConnectionPool>(uri->getHost(), 
            vhost, uri->getPort(), maxSize, 
            maxAliveTime, maxRequest,
            ::strncasecmp(uri->getScheme().data(), 
                "https", uri->getScheme().size()) == 0);
}

HttpConnectionPool::HttpConnectionPool(StringArg host,
                                    StringArg vhost,
                                    uint32_t port,
                                    uint32_t maxSize,
                                    uint32_t maxAliveTime,
                                    uint32_t maxRequest,
                                    bool isHttps) : 
    host_(host),
    vhost_(vhost),
    port_(port ? port : (isHttps ? 443 : 80)),
    maxSize_(maxSize),
    maxAliveTime_(maxAliveTime),
    maxRequest_(maxRequest),
    isHttps_(isHttps) {
}

HttpConnection* HttpConnectionPool::getConnection() {
    uint64_t nowMillionSeconds = nemo::GetCurrentMillionSeconds();
    HttpConnection* validConnection = nullptr;
    {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        for (auto iter = connections_.begin();
            iter != connections_.end();) {
            HttpConnection* connection = iter->get();
            if (!connection->isConnect() || 
                connection->createTime_ + maxAliveTime_ > nowMillionSeconds) {
                connections_.erase(iter++);
            } else {
                validConnection = connection;
                break;
            }
        }
    }

    if(!validConnection) {
        IpAddress::UniquePtr addr = Address::LookupAnyIPAddress(host_);
        if(!addr) {
            NEMO_LOG_ERROR(systemLogger) << "get addr fail: " << host_;
            return nullptr;
        }
        addr->setPort(port_);
        Socket::UniquePtr sock;
        if(isHttps_) {
            sock = SecureSocket::CreateTcp(addr.get());
        } else {
            sock = Socket::CreateTcp(addr.get());
        } 
        if(!sock) {
            NEMO_LOG_ERROR(systemLogger) << "create sock fail: " << *addr;
            return nullptr;
        }
        if(!sock->connect(addr.get())) {
            NEMO_LOG_ERROR(systemLogger) << "sock connect fail: " << *addr;
            return nullptr;
        }

        HttpConnection::UniquePtr newConnection = 
            std::make_unique<HttpConnection>(std::move(sock));

        validConnection = newConnection.get();
        connections_.emplace_back(std::move(newConnection));
    }
    return validConnection;
}

HttpResult::UniquePtr HttpConnectionPool::doRequest(HttpMethod method, 
        StringArg uriStr, 
        uint64_t timeoutMillionSeconds, 
        const std::map<String, String>& headers, 
        StringArg body) {
    HttpRequest::UniquePtr request = std::make_unique<HttpRequest>();
    request->setPath(uriStr);
    request->setMethod(method);
    request->setClose(true);
    bool hasHost = false;
    for(const auto& i : headers) {
        if(::strncasecmp(i.first.data(), "connection", i.first.size()) == 0) {
            if(::strncasecmp(i.second.data(), "keep-alive", i.first.size()) == 0) {
                request->setClose(false);
            }
            continue;
        }

        if(!hasHost && ::strncasecmp(i.first.data(), "host", i.first.size()) == 0) {
            hasHost = !i.second.empty();
        }

        request->setHeader(i.first, i.second);
    }
    if(!request) {
        if(vhost_.empty()) {
            request->setHeader("Host", host_);
        } else {
            request->setHeader("Host", vhost_);
        }
    }
    request->setBody(body);
    return doRequest(request.get(), timeoutMillionSeconds);
}

HttpResult::UniquePtr HttpConnectionPool::doRequest(HttpMethod method, 
        const Uri* uri, 
        uint64_t timeoutMillionSeconds,
        const std::map<String, String>& headers, 
        StringArg body) {
    std::stringstream ss;
    ss << uri->getPath()
       << (uri->getQuery().empty() ? "" : "?")
       << uri->getQuery()
       << (uri->getFragment().empty() ? "" : "#")
       << uri->getFragment();
    return doRequest(method, ss.str(), timeoutMillionSeconds, headers, body);
}

HttpResult::UniquePtr HttpConnectionPool::doRequest(HttpRequest* request, 
                                uint64_t timeoutMillionSeconds) {
    HttpConnection* connection = getConnection();
    if(!connection) {
        String errMsg = " port:" + LexicalCast<String>(port_);
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::POOL_GET_CONNECTION, 
            nullptr, errMsg);
    }
    Socket* sock = connection->getSocket();
    if(!sock) {
        String errMsg = "pool host:" + host_ + " port:" + LexicalCast<String>(port_);
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::POOL_INVALID_CONNECTION, 
            nullptr, errMsg);
    }
    sock->setRecvTimeout(timeoutMillionSeconds);
    int ret = connection->sendRequest(request);
    if(ret == 0) {
        String errMsg = "send request closed by peer: " + sock->getRemoteAddress()->toString();
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::SEND_CLOSE_BY_PEER, 
            nullptr, errMsg);
    }
    if(ret < 0) {
        String errMsg = "send request socket error errno=" + 
                        LexicalCast<String>(errno) + 
                        " errstr=" + String(strerror(errno));
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::SEND_SOCKET_ERROR, 
            nullptr, errMsg);
    }
    HttpResponse::UniquePtr response = connection->recvResponse();
    if(!response) {
        String errMsg = "recv response timeout: " + 
                        sock->getRemoteAddress()->toString() + 
                        " timeout_ms:" + std::to_string(timeoutMillionSeconds);
        return std::make_unique<HttpResult>(HttpResult::ErrorCode::TIMEOUT, 
            nullptr, errMsg);
    }

    return std::make_unique<HttpResult>(HttpResult::ErrorCode::OK, std::move(response), "ok");
}

} // namespace io
} // namespace net
} // namespace nemo