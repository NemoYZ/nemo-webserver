#include <execinfo.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "util/util.h"
#include "common/types.h"
#include "log/log.h"
#include "coroutine/task.h"

namespace nemo {

static Logger::SharedPtr gSystemLogger = NEMO_LOG_NAME("system");

pid_t GetCurrentThreadId() {
    return ::syscall(SYS_gettid); //also pthread_self()
}

uint64_t GetCurrentTaskId() {
    coroutine::Task* currentTask = coroutine::Task::GetCurrentTask();
    return currentTask ? currentTask->getId() : 0;
}

void Backtrace(std::vector<String>& bt, int size, int skip) {
    void** array = reinterpret_cast<void**>(::malloc(sizeof(void*) * size));
    size_t sz = ::backtrace(array, size);

    char** strings = backtrace_symbols(array, sz);
    if (nullptr == strings) {
        NEMO_LOG_ERROR(gSystemLogger) << "backtrace_symbols error";
    }

    for (size_t i = skip; i < sz; ++i) {
        bt.push_back(strings[i]);
    }

    ::free(strings);
    ::free(array);
}

String BacktraceToString(int size, int skip, StringArg prefix) {
    std::vector<String> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for(decltype(bt.size()) i = 0; i < bt.size(); ++i) {
        ss << prefix << bt[i] << "\n";
    }
    return ss.str();
}

uint64_t GetCurrentMillionSeconds() {
    struct timeval tv;
    ::gettimeofday(&tv, nullptr);
    /*秒数*1000 + 微秒数 / 1000就得到总的毫秒数*/
    return tv.tv_sec * 1000ul  + tv.tv_usec / 1000;
}

uint32_t EncodeZigzag32(const int32_t& v) {
	/**
	 * 把符号存储在最低的一位
	 * 正数的最低位肯定为0，负数的最低位肯定为1
	 * 问题1：既然目的是为了把负数的最低位变为1，那为什么不选择+1，而是选择-1？
	 *	1、极端情况，压缩std::numeric_limits<int32_t>::min()，
	 *		如果选择-1，结果会得到std::numeric_limits<uint32_t>::max()，
	 *		如果选择+1，结果会得到1，这与0的压缩结果一样了, 肯定是不行的
	 *  2、把二进制原码转为补码，就是对二进制取反再+1，这里再-1相当于仅仅
	 *		对二进制取反，在解码的时候会方便很多
	 */
    if(v < 0) {
        return (static_cast<uint32_t>(-v)) * 2 - 1;
    } else {
        return v * 2;
    }
}

uint64_t EncodeZigzag64(const int64_t& v) {
    if(v < 0) {
        return (static_cast<uint32_t>(-v)) * 2 - 1;
    } else {
        return v * 2;
    }
}

int32_t DecodeZigzag32(const uint32_t& v) {
	/**
	 * 如果v的最低位是1，代表压缩前的v是一个负数 -(v & 1) 的结果为全1
	 * 	那么 (v >> 1) ^ 全1的结果为把v >> 1的值二进制取反，就可以得到
	 *	编码前的数字
	 * 如果v的最低位是0，代表压缩前的v是一个整数 -(v & 1) 的结果为全0
	 * 	那么 (v >> 1) ^ 全0的结果和v >> 1的值是一样的
	 */
    return (v >> 1) ^ -(v & 1);
}

int64_t DecodeZigzag64(const uint64_t& v) {
    return (v >> 1) ^ -(v & 1);
}

String Format(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    return Format(format, ap);
}

String Format(const char* format, va_list ap) {
    char* buf = nullptr;
    int len = ::vasprintf(&buf, format, ap);
    if (-1 == len) {
        return "";
    }
    String result(buf, len);
    ::free(buf);
    return result;
}

String Time2Str(time_t ts, StringArg format) {
    struct tm tm;
    ::localtime_r(&ts, &tm);
    char buf[64];
    ::strftime(buf, sizeof(buf), format.data(), &tm);
    return buf;
}

time_t Str2Time(StringArg str, StringArg format) {
    struct tm t;
    MemoryZero(&t, sizeof(t));
    if(!strptime(str.data(), format.data(), &t)) {
        return 0;
    }
    return mktime(&t);
}

void ListFiles(std::vector<String>& files, const String& path, 
    const std::unordered_set<String>& suffixes) {
    if (::access(path.data(), F_OK | R_OK)) { //文件不存在或者没有读的权限
        NEMO_LOG_ERROR(gSystemLogger) << "file not eixts or denied to access, path = " 
                                      << path;
        return;
    }
    struct stat st;
    int result = ::stat(path.data(), &st);
    if (result) {
        NEMO_LOG_ERROR(gSystemLogger) << "open file faild, returned " << result
                << " errno = " << errno << " strerr = " << strerror(errno);
        return;
    }
    if (S_ISDIR(st.st_mode)) { //是一个目录
        DIR* dir = ::opendir(path.data());
        if (nullptr == dir) {
            NEMO_LOG_ERROR(gSystemLogger) << "open dir faile, path = " << path
                    << " errno = " << errno << " errstr = " << strerror(errno);
            return;
        }

        struct dirent* d = nullptr;
        while ((d = ::readdir(dir)) != nullptr) {
            if (DT_DIR == d->d_type) {
                //跳过本目录和上级目录
                if (!::strcmp(d->d_name, ".") ||
                    !::strcmp(d->d_name, "..")) {
                    continue;
                }
                //小心尾巴的 "/"
                if (path.back() == '/') {
                    ListFiles(files, path + d->d_name, suffixes);
                } else {
                    ListFiles(files, path + "/" + d->d_name, suffixes);
                }
            } else if (DT_REG == d->d_type) {
                if (suffixes.empty()) {
                    files.push_back(path + "/" + d->d_name);
                } else {
                    String filename(d->d_name);
                    String::size_type pos = filename.find_last_of('.');
                    if (String::npos == pos) {
                        return;
                    }
                    String suffix = filename.substr(pos + 1); //'.' not included
                    if (suffixes.find(suffix) != suffixes.end()) {
                        files.push_back(path + "/" + filename);
                    }
                }
            }
        } 
        ::closedir(dir);
    } else if (S_ISREG(st.st_mode)){ //普通文件
        String::size_type pos = path.find_last_of('.');
        if (String::npos == pos) {
            return;
        }
        String suffix = path.substr(pos + 1); //'.' not included
        if (suffixes.find(suffix) != suffixes.end()) {
            files.push_back(path);
        }
    }
}

size_t Hex2Str(char* buf, uintptr_t v) {
    static const char hexDigits[] = "0123456789ABCDEF";
    static_assert(sizeof(hexDigits) == 17, "wrong number of digits");
    
    uintptr_t tmp = v;
    char* p = buf;

    do {
        int lsd = static_cast<int>(tmp % 16);
        tmp /= 16;
        *p++ = hexDigits[lsd];
    } while (tmp != 0);

    if (v < 0) {
        *p++ = '-';
    }
    *p = '\0';
    std::reverse(buf, p);

    return p - buf;
}

} //namespace nemo

