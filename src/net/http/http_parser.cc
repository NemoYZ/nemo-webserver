#include "net/http/http_parser.h"

#include <string.h>

#include "log/log.h"
#include "common/config.h"
#include "common/macro.h"

namespace nemo {
namespace net {
namespace http {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

static ConfigVar<size_t>* httpRequestBufferSizeConfig =
    Config::Lookup("http.request.buffer_size",
                    static_cast<size_t>(4 * 1024), 
                    "http request buffer size");

static ConfigVar<size_t>* httpRequestMaxBodySizeConfig =
    Config::Lookup("http.request.max_body_size",
                    static_cast<size_t>(64 * 1024 * 1024), 
                    "http request max body size");

static ConfigVar<size_t>* httpResponseBufferSizeConfig =
    Config::Lookup("http.response.buffer_size",
                    static_cast<size_t>(4 * 1024), 
                    "http response buffer size");

static ConfigVar<size_t>* httpResponseMaxBodySizeConfig =
    Config::Lookup("http.response.max_body_size",
                    static_cast<size_t>(64 * 1024 * 1024), 
                    "http response max body size");

static size_t httpRequestBufferSize = 0;     //request最大大小
static size_t httpRequestMaxBodySize = 0;    //request body最大大小
static size_t httpResponseBufferSize = 0;    //response 最大大小
static size_t httpResponseMaxBodySize = 0;   //response body最大大小

size_t HttpRequestParser::GetHttpRequestBufferSize() {
    return httpRequestBufferSize;
}

size_t HttpRequestParser::GetHttpRequestMaxBodySize() {
    return httpRequestMaxBodySize;
}

size_t HttpResponseParser::GetHttpResponseBufferSize() {
    return httpResponseBufferSize;
}

size_t HttpResponseParser::GetHttpResponseMaxBodySize() {
    return httpResponseMaxBodySize;
}


namespace {
struct RequestSizeIniter {
    RequestSizeIniter() {
        httpRequestBufferSize = httpRequestBufferSizeConfig->getValue();
        httpRequestMaxBodySize = httpRequestMaxBodySizeConfig->getValue();
        httpResponseBufferSize = httpResponseBufferSizeConfig->getValue();
        httpResponseMaxBodySize = httpResponseMaxBodySizeConfig->getValue();

        httpRequestBufferSizeConfig->addListener([](const size_t& oldVal, const size_t& newVal) {
            static_cast<void>(oldVal);
            httpRequestBufferSize = newVal;
        });

        httpRequestMaxBodySizeConfig->addListener([](const size_t& oldVal, const size_t& newVal) {
            static_cast<void>(oldVal);
            httpRequestMaxBodySize = newVal;
        });

        httpResponseBufferSizeConfig->addListener([](const size_t& oldVal, const size_t& newVal) {
            static_cast<void>(oldVal);
            httpRequestBufferSize = newVal;
        });

        httpResponseMaxBodySizeConfig->addListener([](const size_t& oldVal, const size_t& newVal) {
            static_cast<void>(oldVal);
            httpResponseMaxBodySize = newVal;
        });
    }
};

static RequestSizeIniter requestSizeIniter;

} //global namespace

static void OnRequestMethod(void *data, const char *at, size_t length) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    HttpMethod method = String2HttpMethod(StringArg(at, length));

    if(HttpMethod::INVALID_METHOD == method) {
        NEMO_LOG_WARN(systemLogger) << "invalid http request method: "
            << StringArg(at, length);
        parser->setError(HttpParserError::INVALID_METHOD);
        return;
    }
    parser->getRequest()->setMethod(method);
}

static void OnRequestUri(void *data, const char *at, size_t length) {
}

static void OnRequestFragment(void *data, const char *at, size_t length) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getRequest()->setFragment(StringArg(at, length));
}

static void OnRequestPath(void *data, const char *at, size_t length) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getRequest()->setPath(StringArg(at, length));
}

static void OnRequestQuery(void *data, const char *at, size_t length) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getRequest()->setQuery(StringArg(at, length));
}

static void OnRequestVersion(void *data, const char *at, size_t length) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    HttpVersion version;
    bool hasError = false;

    if (::strncmp(at, "HTTP/1.", length - 1) == 0) {
        if (at[length - 1] == '1') {
            version = HttpVersion::HTTP11;
        } else if (at[length - 1] == '0') {
            version = HttpVersion::HTTP10;
        } else {
            hasError = true;
        }
    } else {
        hasError = true;
    }
    
    if (hasError) {
        NEMO_LOG_WARN(systemLogger) << "invalid http request version: "
            << StringArg(at, length);
        parser->setError(HttpParserError::INVALID_VERSION);
        return;
    }
    
    parser->getRequest()->setVersion(version);
}

static void OnRequestHeaderDone(void *data, const char *at, size_t length) {
}

static void OnRequestHttpField(void *data, const char *field, size_t flen
                           ,const char *value, size_t vlen) {
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    if(0 == flen) {
        NEMO_LOG_WARN(systemLogger) << "invalid http request field length == 0";
        //parser->setError(1002);
        return;
    }
    parser->getRequest()->setHeader(String(field, flen),
        String(value, vlen));
}

HttpRequestParser::HttpRequestParser() :
    request_(std::make_unique<HttpRequest>()),
    error_(HttpParserError::OK) {
    http_parser_init(&parser_);
    parser_.request_method = OnRequestMethod;
    parser_.request_uri = OnRequestUri;
    parser_.fragment = OnRequestFragment;
    parser_.request_path = OnRequestPath;
    parser_.query_string = OnRequestQuery;
    parser_.http_version = OnRequestVersion;
    parser_.header_done = OnRequestHeaderDone;
    parser_.http_field = OnRequestHttpField;
    parser_.data = this;
}

uint64_t HttpRequestParser::getContentLength() {
    return request_->getHeaderAs<size_t>("content-length", 0);
}

//1: 成功
//-1: 有错误
//>0: 已处理的字节数，且data有效数据为len - v;
size_t HttpRequestParser::execute(char* data, size_t len) {
    size_t offset = http_parser_execute(&parser_, data, len, 0);
    //把已经解析的覆盖掉，这样下次继续解析的时候传data就行了，不必传data + offset
    ::memmove(data, data + offset, (len - offset));
    return offset;
}

int HttpRequestParser::isFinished() {
    return http_parser_finish(&parser_);
}

static void OnResponseReason(void *data, const char *at, size_t length) {
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    parser->getResponse()->setReason(std::string(at, length));
}

static void OnResponseStatus(void *data, const char *at, size_t length) {
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    HttpStatus status = (HttpStatus)(atoi(at));
    parser->getResponse()->setStatus(status);
}

static void OnResponseChunk(void *data, const char *at, size_t length) {
}

static void OnResponseVersion(void *data, const char *at, size_t length) {
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    HttpVersion version;
    bool hasError = false;

    if (strncmp(at, "HTTP/1.", length - 1) == 0) {
        if (at[length - 1] == '1') {
            version = HttpVersion::HTTP11;
        } else if (at[length - 1] == '0'){
            version = HttpVersion::HTTP10;
        } else {
            hasError = true;
        }
    } else {
        hasError = true;
    }

    if (hasError) {
        NEMO_LOG_WARN(systemLogger) << "invalid http response version: "
            << std::string(at, length);
            parser->setError(HttpParserError::INVALID_VERSION);
            return;
    }
    
    parser->getResponse()->setVersion(version);
}

static void OnResponseHeaderDone(void *data, const char *at, size_t length) {
}

static void OnResponseLastChunk(void *data, const char *at, size_t length) {
}

static void OnResponseHttpField(void *data, const char *field, size_t flen
                           ,const char *value, size_t vlen) {
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    if(0 == flen) {
        NEMO_LOG_WARN(systemLogger) << "invalid http response field length == 0";
        return;
    }
    parser->getResponse()->setHeader(String(field, flen),
        String(value, vlen));
}

HttpResponseParser::HttpResponseParser() :
    response_(std::make_unique<HttpResponse>()),
    error_(HttpParserError::OK) {
    httpclient_parser_init(&parser_);
    parser_.reason_phrase = OnResponseReason;
    parser_.status_code = OnResponseStatus;
    parser_.chunk_size = OnResponseChunk;
    parser_.http_version = OnResponseVersion;
    parser_.header_done = OnResponseHeaderDone;
    parser_.last_chunk = OnResponseLastChunk;
    parser_.http_field = OnResponseHttpField;
    parser_.data = this;
}

size_t HttpResponseParser::execute(char* data, size_t len, bool chunck) {
    if(chunck) {
        httpclient_parser_init(&parser_);
    }
    size_t offset = httpclient_parser_execute(&parser_, data, len, 0);

    ::memmove(data, data + offset, (len - offset));
    return offset;
}

} // namespace http
} // namespace net
} // namespace nemo