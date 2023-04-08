#include "coroutine/coroutine.h"

namespace nemo {
namespace coroutine {

SyntaxHelper::SyntaxHelper(Token) :
    scheduler_(std::make_shared<Scheduler>("global")) {
}

} // namespace coroutine
} // namespace nemo