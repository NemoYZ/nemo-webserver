#include "log/log_formatter.h"

#include <iostream>

#include "log/log_event.h"
#include "log/logger.h"

namespace nemo {

LogFormatter::LogFormatter(StringArg pattern) : 
    hasError_(false),
    items_(),
    pattern_(pattern.data(), pattern.size()) {
    parse();
}

String LogFormatter::format(std::shared_ptr<Logger> logger,
                            LogLevel level,
                            std::shared_ptr<LogEvent> event) {
    std::stringstream ss;
    //std::cout << "items_.size()=" << std::flush << items_.size() << std::endl;
    for(auto& item : items_) {
        item->format(ss, logger, level, event);
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream& os,
                                   std::shared_ptr<Logger> logger,
                                   LogLevel level,
                                   std::shared_ptr<LogEvent> event) {
    for(auto& item : items_) {
        item->format(os, logger, level, event);
    }
    return os;
}

class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const String& str = "") {
    }
    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << event->getContent();  //普通文本直接输出
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::MESSAGE;
    }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const String& str = "") {
    }
    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << LexicalCast<String>(level); //先把日志级别转化为对应的字符串再输出
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::LOG_LEVEL;
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const String& str = "") {
    }
    void format(std::ostream& os, 
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << event->getElapse();  //数字直接输出
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::ELAPSE;
    }
};

class NameFormatItem : public LogFormatter::FormatItem {
public:
    NameFormatItem(const String& str = "") {
    }
    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << event->getLogger()->getName(); //字符串直接输出
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::LOG_NAME;
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const String& str = "") {
    }
    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << event->getThreadId(); //数字直接输出
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::THREAD_ID;
    }
};

class TaskIdFormatItem : public LogFormatter::FormatItem {
public:
    TaskIdFormatItem(const String& str = "") {
    }
    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << event->getTaskId(); //数字直接输出
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::TASK_ID;
    }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem  {
public:
    ThreadNameFormatItem(const String& str = "") {
    }
    void format(std::ostream& os, 
                std::shared_ptr<Logger> logger, 
                LogLevel level, 
                std::shared_ptr<LogEvent> event) override {
        os << event->getThreadName(); //字符串直接输出
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::THREAD_NAME;
    }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(StringArg format = "%Y-%m-%d %H:%M:%S") :
        format_(format.data(), format.size()) {
        if(format_.empty()) {
            format_ = "%Y-%m-%d %H:%M:%S";
        }
    }

    void format(std::ostream& os, 
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override {
        os << event->getTimestamp().toFormattedString(format_);
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::DATE;
    }

private:
    String format_;
};

class FilenameFormatItem : public LogFormatter::FormatItem {
public:
    FilenameFormatItem(const String& str = "") {
    }
    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << event->getFileName(); //文件流，可以直接传给其他流
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::FILENAME;
    }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const String& str = "") {
    }
    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << event->getLine();  //行号，直接输出
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::LINE;
    }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const String& str = "") {
    }
    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << std::endl; //换行
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::NEW_LINE;
    }
};


class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(StringArg str) :
        str_(str.data(), str.size()) {
    }

    StringFormatItem(String&& str) :
        str_(std::move(str)) {
    }

    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << str_; //直接打印字符串
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::STRING;
    }

private:
    String str_;
};

class TabFormatItem : public LogFormatter::FormatItem {
public:
    TabFormatItem(const String& str) {
    }
    
    void format(std::ostream& os,
                std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event) override { 
        os << "\t"; //直接打印制表符
    }

    virtual FormatItemType getType() const override {
        return FormatItemType::TAB;
    }
};

//%xxx %xxx{xxx} %%
void LogFormatter::parse() {
    //str, format, type
    //type为0就表示普通文本，type为1表示转义字符
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr;   //普通文本
    for(size_t i = 0; i < pattern_.size(); ++i) {
        if(pattern_[i] != '%') {           //普通的文本
            nstr.append(1, pattern_[i]);
            continue;
        }
        
        //到达这里，那么mPattern[i]肯定为%
        if((i + 1) < pattern_.size()) {    
            if(pattern_[i + 1] == '%') {   //%%
                nstr.append(1, '%');
                continue;
            }
        }
        
        //其余的输出格式，比如%p, %n等
        size_t n = i + 1;      //从%下一个位置开始
        int fmtStatus = 0;     //1表示已经遇到'{', 0表示还没有遇到'{'或者遇到了'}'
        size_t fmtBegin = 0;   //'{'开始的下标

        std::string str;       //普通转义
        std::string fmt;       //{}里面的内容
        while(n < pattern_.size()) {
            if(!fmtStatus &&                
               !isalpha(pattern_[n]) &&
               pattern_[n] != '{' && 
               pattern_[n] != '}') { //可能扫描到了%Y-%M之间的 '-'，或者下一个%
                str = pattern_.substr(i + 1, n - i - 1);
                break;
            }
            if(0 == fmtStatus && '{' == pattern_[n]) {
                str = pattern_.substr(i + 1, n - i - 1);
                fmtStatus = 1; //解析格式
                fmtBegin = n;  
                ++n;
                continue;
            } else if (1 == fmtStatus && '}' == pattern_[n]) { //fmtStatus == 1
                fmt = pattern_.substr(fmtBegin + 1, n - fmtBegin - 1);
                fmtStatus = 0;
                ++n;
                break;
            }

            ++n;
            if(pattern_.size() == n) {
                if(str.empty()) {
                    str = pattern_.substr(i + 1);
                }
            }
        }

        if(fmtStatus == 0) { //没有{}
            if(!nstr.empty()) {
                vec.push_back(std::make_tuple(std::move(nstr), std::string(), 0));
                nstr.clear(); //有可能nstr的长度 < 16，move之后并不会清空nstr里的内容(See the implemention of string)
            }
            vec.push_back(std::make_tuple(std::move(str), fmt, 1)); 
            i = n - 1; //注意循环结束以后i会++，所以这里要在n的基础上 - 1
        } else {    //fmtStatus == 1，'{'的数量多于'}'
            std::cout << "pattern parse error: " << pattern_ << " - " << pattern_.substr(i) << std::endl;
            hasError_ = true;
            vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
        }
    }

    if(!nstr.empty()) {
        vec.push_back(std::make_tuple(std::move(nstr), "", 0));
    }
    static std::unordered_map<std::string, std::function<FormatItem*(const String& str)> > sFormatItems = {
#define XX(str, Item)\
        {std::string(#str), [](const std::string& fmt) { return new Item(fmt);}}

        XX(m, MessageFormatItem),   //m 消息
        XX(p, LevelFormatItem),     //p 日志级别
        XX(r, ElapseFormatItem),    //r 累计毫米数
        XX(c, NameFormatItem),      //c 日志名词
        XX(t, ThreadIdFormatItem),  //t 线程id
        XX(n, NewLineFormatItem),   //n 换行
        XX(d, DateTimeFormatItem),  //d 时间
        XX(f, FilenameFormatItem),  //f 文件名
        XX(l, LineFormatItem),      //l 行号
        XX(T, TabFormatItem),       //T Tab
        XX(F, TaskIdFormatItem),   //F 协程id
        XX(N, ThreadNameFormatItem),//N 线程名词
#undef XX
    };

    for(auto& i : vec) {
        if(0 == std::get<2>(i)) {   //type为0
            items_.emplace_back(new StringFormatItem(std::move(std::get<0>(i))));
        } else {
            auto iter = sFormatItems.find(std::get<0>(i));
            if(iter == sFormatItems.end()) {  //没有这种格式
                items_.emplace_back(new StringFormatItem("<<unknow format: %" + std::get<0>(i) + ">>"));
                hasError_ = true;
            } else {
                items_.emplace_back(iter->second(std::get<1>(i)));
            }
        }
    }
}

} //namespace nemo