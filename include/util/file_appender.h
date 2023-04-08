#pragma once

#include <stdio.h>

#include <memory>
#include <fstream>

#include "container/buffer.h"
#include "common/types.h"

namespace nemo {
namespace file_util {

/**
 * @brief 文件追加类
 * 
 */
class FileAppender {
public:
    typedef std::shared_ptr<FileAppender> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<FileAppender> UniquePtr; ///< 智能指针定义

public:
    FileAppender(StringArg filename);
    ~FileAppender();

public:
    void append(const void* msg, size_t len);
    void flush();
    bool isOpen() const noexcept { return fp_; }
    off_t getWrittenBytes() const noexcept { return writtenBytes_; }

private:
    size_t write(const void* msg, size_t len);

private:
    FILE* fp_;
    Buffer::UniquePtr buffer_;
    off_t writtenBytes_;
};


} //namespace file_util
} //namespace nemo