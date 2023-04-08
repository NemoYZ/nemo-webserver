#include "net/http/http_status.h"

namespace nemo {
namespace net {
namespace http {

const char* HttpStatus2String(const HttpStatus& status) {
    switch(status) {
#define XX(code, name, msg) \
        case HttpStatus::name: \
            return #msg;
        HTTP_STATUS_MAP(XX);
#undef XX
        default:
            return "<unknown>";
    }
    return "<unknown>";
}

} // namespace http
} // namespace net
} // namespace nemo