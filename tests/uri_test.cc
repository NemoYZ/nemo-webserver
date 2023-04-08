#include "net/uri.h"

#include "log/log.h"

static nemo::Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");

int main(int argc, char** argv) {
    nemo::net::Uri::UniquePtr uri = nemo::net::Uri::Create("https://www.baidu.com");
    if (!uri) {
        NEMO_LOG_ERROR(rootLogger) << "create uri failed";
    } else {
        NEMO_LOG_DEBUG(rootLogger) << "uri=" << uri->toString();
    }

    return 0;
}