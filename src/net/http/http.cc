#include "net/http/http.h"

namespace nemo {
namespace net {
namespace http {

HttpRequest::HttpRequest(HttpVersion version, bool close) :
    method_(HttpMethod::GET),
    version_(version),
    close_(close),
    webSocket_(false),
    parserParamFlag_(0),
    path_("/") {
}

bool HttpRequest::createResponse(std::unique_ptr<HttpResponse>& response) {
    response.reset(new HttpResponse(getVersion(), isClose()));
    return true;
}

String HttpRequest::getHeader(const String& key, const String& defaultVal) const {
    auto iter = headers_.find(key);
    return headers_.end() == iter ? defaultVal : iter->second;
}

String HttpRequest::getParam(const String& key, const String& defaultVal) {
    initQueryParam();
    initBodyParam();
    auto iter = params_.find(key);
    return params_.end() == iter ? defaultVal : iter->second;
}

String HttpRequest::getCookie(const String& key, const String& defaultVal) {
    initCookies();
    auto iter = cookies_.find(key);
    return cookies_.end() == iter ? defaultVal : iter->second;
}

void HttpRequest::setHeader(const String& key, const String& val) {
    headers_[key] = val;
}

void HttpRequest::setHeader(const String& key, String&& val) {
    headers_[key] = std::move(val);
}

void HttpRequest::setParam(const String& key, const String& val) {
    params_[key] = val;
}

void HttpRequest::setParam(const String& key, String&& val) {
    params_[key] = std::move(val);
}

void HttpRequest::setCookie(const String& key, const String& val) {
    cookies_[key] = val;
}

void HttpRequest::setCookie(const String& key, String&& val) {
    cookies_[key] = std::move(val);
}

void HttpRequest::delHeader(const String& key) {
    headers_.erase(key);
}

void HttpRequest::delParam(const String& key) {
    params_.erase(key);
}

void HttpRequest::delCookie(const String& key) {
    cookies_.erase(key);
}

bool HttpRequest::hasHeader(const String& key, String* val) {
    auto iter = headers_.find(key);
    if(headers_.end() == iter) {
        return false;
    }
    if(val) {
        *val = iter->second;
    }
    return true;
}

bool HttpRequest::hasParam(const String& key, String* val) {
    initQueryParam();
    initBodyParam();
    auto iter = params_.find(key);
    if(params_.end() == iter) {
        return false;
    }
    if(val) {
        *val = iter->second;
    }
    return true;
}

bool HttpRequest::hasCookie(const String& key, String* val) {
    initCookies();
    auto iter = cookies_.find(key);
    if(cookies_.end() == iter) {
        return false;
    }
    if(val) {
        *val = iter->second;
    }
    return true;
}

std::ostream& HttpRequest::dump(std::ostream& os) const {
    /**
     * GET /uri HTTP/1.1
     * Host: www.baidu.com
     * 
     */
    os << HttpMethod2String(method_) << " "
       << path_
       << (query_.empty() ? "" : "?")
       << query_
       << (fragment_.empty() ? "" : "#")
       << fragment_
       << " HTTP/"
       << ((uint32_t)((uint8_t)version_ >> 4))
       << "."
       << ((uint32_t)((uint8_t)version_ & 0x0F))
       << "\r\n";
    if(!webSocket_) {
        os << "connection: " << (close_ ? "close" : "keep-alive") << "\r\n";
    }
    for(auto& header : headers_) {
        if(!webSocket_ && 
            ::strncasecmp(header.first.data(), 
                "connection", header.first.size()) == 0) {
            continue;
        }
        os << header.first << ": " << header.second << "\r\n";
    }

    if(!body_.empty()) {
        os << "content-length: " << body_.size() << "\r\n\r\n"
           << body_;
    } else {
        os << "\r\n";
    }

    return os;
}

String HttpRequest::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

void HttpRequest::init() {
    String connection = getHeader("connection");
    if(!connection.empty()) {
        if(strcasecmp(connection.c_str(), "keep-alive") == 0) {
            close_ = false;
        } else {
            close_ = true;
        }
    }
}

void HttpRequest::initParam() {
    
}

void HttpRequest::initQueryParam() {

}

void HttpRequest::initBodyParam() {

}

void HttpRequest::initCookies() {

}


HttpResponse::HttpResponse(HttpVersion version, bool close) :
    status_(HttpStatus::OK),
    version_(version),
    close_(close),
    webSocket_(false)  {
}

String HttpResponse::getHeader(const String& key, const String& defaultVal) const {
    auto iter = headers_.find(key);
    return headers_.end() == iter ? defaultVal : iter->second;
}

void HttpResponse::setHeader(const String& key, const String& val) {
    headers_[key] = val;
}

void HttpResponse::setHeader(const String& key, String&& val) {
    headers_[key] = std::move(val);
}

void HttpResponse::delHeader(const String& key) {
    headers_.erase(key);
}

std::ostream& HttpResponse::dump(std::ostream& os) const {
    /**
     * HTTP1.1 200 OK
     *
     */
    os << "HTTP/"
       << ((uint32_t)((uint8_t)version_ >> 4))
       << "."
       << ((uint32_t)((uint8_t)version_ & 0x0F))
       << " "
       << (uint32_t)status_
       << " "
       << (reason_.empty() ? HttpStatus2String(status_) : reason_)
       << "\r\n";

    for(auto& header : headers_) {
        if(!webSocket_ && strcasecmp(header.first.c_str(), "connection") == 0) {
            continue;
        }
        os << header.first << ": " << header.second << "\r\n";
    }
    for(auto& cookie : cookies_) {
        os << "Set-Cookie: " << cookie << "\r\n";
    }
    if(!webSocket_) {
        os << "connection: " << (close_ ? "close" : "keep-alive") << "\r\n";
    }
    if(!body_.empty()) {
        os << "content-length: " << body_.size() << "\r\n\r\n"
           << body_;
    } else {
        os << "\r\n";
    }
    
    return os;
}

String HttpResponse::toString() const {
    std::stringstream ss;
    dump(ss);

    return ss.str();
}

void HttpResponse::setRedirect(const String& uri) {
}

void HttpResponse::setCookie(const String& key, const String& val,
                time_t expired, const String& path,
                const String& domain, bool secure) {

}

std::ostream& operator<<(std::ostream& os, const HttpRequest& request) {
    return request.dump(os);
}

std::ostream& operator<<(std::ostream& os, const HttpResponse& response) {
    return response.dump(os);
}

} // namespace http
} // namespace net
} // namespace nemo