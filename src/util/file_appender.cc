#include "util/file_appender.h"

#include <sys/param.h>

#include "common/macro.h"
#include "system/parameter.h"

namespace nemo {
namespace file_util {

FileAppender::FileAppender(StringArg filename) : 
    fp_(fopen(filename.data(), "ae")),  // 'e' for O_CLOEXEC
    buffer_(new Buffer(kBytesPerPage, true)),
    writtenBytes_(0) {
    NEMO_ASSERT(fp_ != nullptr);
    flush();
    setbuffer(fp_, buffer_->data(), buffer_->capacity());
}

FileAppender::~FileAppender() {
    flush();
    fclose(fp_);
}

void FileAppender::append(const void* msg, const size_t len) {
    size_t written = 0;

    while (written != len) {
        size_t remain = len - written;
        size_t n = write(static_cast<const char*>(msg) + written, remain);
        if (n != remain) {
            int err = ferror(fp_);
            if (err) {
                fprintf(stderr, "AppendFile::append() failed %s\n", strerror(err));
                break;
            }
        }
        written += n;
    }

    writtenBytes_ += written;
}

void FileAppender::flush() {
    fflush(fp_);
}

size_t FileAppender::write(const void* msg, size_t len) {
    return fwrite_unlocked(msg, sizeof(char), len, fp_);
}

} //namespace file_util
} //namespace nemo