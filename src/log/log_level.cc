#include "log/log_level.h"

namespace nemo {

const char* LogLevel2String(LogLevel level) {
    switch(level) {
#define CASE(name) \
    case LogLevel::name: \
        return #name; \
        break;

    CASE(DEBUG);
    CASE(INFO);
    CASE(WARN);
    CASE(ERROR);
    CASE(FATAL);
#undef CASE
    default:
        return "ALL";
    }
    return "ALL";
}

LogLevel String2LogLevel(StringArg str) {
#define IF(level, v) \
    if(::strncasecmp(str.data(), #v, str.size()) == 0) { \
        return level; \
    }
    IF(DEBUG, debug);
    IF(INFO, info);
    IF(WARN, warn);
    IF(ERROR, error);
    IF(FATAL, fatal);

    return LogLevel::ALL;
#undef IF

} //namespace nemo

}