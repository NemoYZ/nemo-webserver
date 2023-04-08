#pragma once

#include <string.h>

#include "common/types.h"

namespace nemo {
namespace string_util {

class CaseInsensitiveLess {
public:
    bool operator()(const StringArg& lhs, const StringArg& rhs) const {
        return ::strcasecmp(lhs.data(), rhs.data()) < 0;
    }
};

} // namepace string_util
} // namespace nemo