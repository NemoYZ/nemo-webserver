#include "net/http/http_method.h"

#include <string.h>

namespace nemo {
namespace net {
namespace http {

HttpMethod String2HttpMethod(StringArg method) {
#define XX(num, name, str)                   \
    if (::strncasecmp(#str, method.data(), method.size()) == 0) { \
        return HttpMethod::name; \
    }
    HTTP_METHOD_MAP(XX);
#undef XX
    return HttpMethod::INVALID_METHOD;
}

const char* HttpMethod2String(const HttpMethod& method) {
    static const char* methodStrings[] = {
#define XX(num, name, string) #string,
    HTTP_METHOD_MAP(XX)
#undef XX
    };
    uint32_t index = (uint32_t)method;
    if(index >= (sizeof(methodStrings) / sizeof(methodStrings[0]))) {
        return "<unknown>";
    }
    return methodStrings[index];
}

} // namespace http
} // namespace net
} // namespace nemo