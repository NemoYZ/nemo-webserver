#include "system/env.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "util/util.h"

namespace nemo {

Env::Env(Token) {
    char link[128]{0};
    char dir[128]{0};
    
    ::sprintf(link, "/proc/%d/exe", getPid());
    ::readlink(link, dir, sizeof(dir));
    execDir_ = dir;

    ::sprintf(link, "/proc/%d/cwd", getPid());
    ::readlink(link, dir, sizeof(dir));
    currWorkDir_ = dir;

    String::size_type pos = execDir_.find_last_of('/');
    workDir_ = execDir_.substr(0, pos);

    char hostName[256]{0};
    if (::gethostname(hostName, sizeof(hostName)) == 0) {
        hostName_ = hostName;
    } else {
        hostName_ = "unknown host";
    }
}

} //namespace nemo