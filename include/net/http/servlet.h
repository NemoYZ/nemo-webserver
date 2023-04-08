#pragma once

#include <memory>
#include <functional>
#include <shared_mutex>
#include <map>
#include <vector>

#include "util/util.h"
#include "common/types.h"

namespace nemo {
namespace net {
namespace http {

class HttpRequest;
class HttpResponse;
class HttpSession;

/**
 * @brief Servlet封装
 */
class HttpServlet {
public:
    typedef std::shared_ptr<HttpServlet> SharedPtr; ///< 智能指针定义 
    typedef std::unique_ptr<HttpServlet> UniquePtr; ///< 智能指针定义

    /**
     * @brief 构造函数
     * @param[in] name 名称
     */
    HttpServlet(StringArg name);

    /**
     * @brief 析构函数
     */
    virtual ~HttpServlet() = default;

    /**
     * @brief 处理请求
     * @param[in] request HTTP请求
     * @param[in] response HTTP响应
     * @param[in] session HTTP连接
     * @return 是否处理成功
     */
    virtual int32_t handle(HttpRequest* request, 
                    HttpResponse* response,
                    HttpSession* session) = 0;
                   
    /**
     * @brief 返回Servlet名称
     */
    const String& getName() const { return name_; }

protected:
    String name_; //名称
};

/**
 * @brief 函数式Servlet
 */
class FunctionServlet : public HttpServlet {
public:
    typedef std::shared_ptr<FunctionServlet> ptr; ///< 智能指针类型定义
    typedef std::function<int32_t (HttpRequest* request, 
                    HttpResponse* response, 
                    HttpSession* session)> Callback; //函数回调类型定义 


    /**
     * @brief 构造函数
     * @param[in] cb 回调函数
     */
    FunctionServlet(const Callback& cb);
    FunctionServlet(Callback&& cb);
    int handle(HttpRequest* request, 
            HttpResponse* response, 
            HttpSession* session) override;
private:
    Callback cb_; //回调函数
};

class IServletCreator {
public:
    typedef std::shared_ptr<IServletCreator> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<IServletCreator> UniquePtr; ///< 智能指针定义

public:
    virtual ~IServletCreator() = default;
    virtual HttpServlet::SharedPtr get() const = 0;
    virtual String getName() const = 0;
};

class HoldServletCreator : public IServletCreator {
public:
    typedef std::shared_ptr<HoldServletCreator> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HoldServletCreator> UniquePtr; ///< 智能指针定义

public:
    HoldServletCreator(const HttpServlet::SharedPtr& servlet);

    HttpServlet::SharedPtr get() const override {
        return servlet_;
    }

    String getName() const override {
        return servlet_->getName();
    }

private:
    HttpServlet::SharedPtr servlet_;
};

template<typename T>
class ServletCreator : public IServletCreator {
public:
    typedef std::shared_ptr<ServletCreator> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<ServletCreator> UniquePtr; ///< 智能指针定义

    HttpServlet::SharedPtr get() const override {
        return HttpServlet::SharedPtr(new T);
    }

    String getName() const override {
        return Demangle<T>();
    }
};

/**
 * @brief Servlet分发器
 */
class ServletDispatcher : public HttpServlet {
public:
    typedef std::shared_ptr<ServletDispatcher> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<ServletDispatcher> UniquePtr; ///< 智能指针定义

    /**
     * @brief 构造函数
     */
    ServletDispatcher();
    int handle(HttpRequest* request,
            HttpResponse* response, 
            HttpSession* session) override;

    /**
     * @brief 添加servlet
     * @param[in] uri uri
     * @param[in] servlet serlvet
     */
    void addServlet(StringArg uri, HttpServlet::UniquePtr&& servlet);

    /**
     * @brief 添加servlet
     * @param[in] uri uri
     * @param[in] cb FunctionServlet回调函数
     */
    void addServlet(StringArg uri, const FunctionServlet::Callback& cb);
    void addServlet(StringArg uri, FunctionServlet::Callback&& cb);

    /**
     * @brief 添加模糊匹配servlet
     * @param[in] uri uri 模糊匹配 /nemo_*
     * @param[in] servlet servlet
     */
    void addGlobServlet(StringArg uri, HttpServlet::UniquePtr&& servlet);

    /**
     * @brief 添加模糊匹配servlet
     * @param[in] uri uri 模糊匹配 /Nemo_*
     * @param[in] cb FunctionServlet回调函数
     */
    void addGlobServlet(StringArg uri, const FunctionServlet::Callback& cb);
    void addGlobServlet(StringArg uri, FunctionServlet::Callback&& cb);

    void addServletCreator(StringArg uri, IServletCreator::UniquePtr&& creator);
    void addGlobServletCreator(StringArg uri, IServletCreator::UniquePtr&& creator);

    template<class T>
    void addServletCreator(StringArg uri) {
        addServletCreator(uri, std::make_unique<ServletCreator<T>>());
    }

    template<class T>
    void addGlobServletCreator(StringArg uri) {
        addGlobServletCreator(uri, std::make_unique<ServletCreator<T> >());
    }

    /**
     * @brief 删除servlet
     * @param[in] uri uri
     */
    void delServlet(const String& uri);

    /**
     * @brief 删除模糊匹配servlet
     * @param[in] uri uri
     */
    void delGlobServlet(StringArg uri);

    /**
     * @brief 返回默认servlet
     */
    HttpServlet* getDefault() const {
        return defaultServlet_.get(); 
    }

    /**
     * @brief 设置默认servlet
     * @param[in] servlet 默认servlet
     */
    void setDefault(HttpServlet::UniquePtr&& servlet) { 
        defaultServlet_ = std::move(servlet); 
    }

    /**
     * @brief 通过uri获取servlet
     * @param[in] uri uri
     * @return 返回对应的servlet
     */
    HttpServlet::SharedPtr getServlet(const String& uri);

    /**
     * @brief 通过uri获取模糊匹配servlet
     * @param[in] uri uri
     * @return 返回对应的servlet
     */
    HttpServlet::SharedPtr getGlobServlet(const String& uri);

    /**
     * @brief 通过uri获取servlet
     * @param[in] uri uri
     * @return 优先精准匹配,其次模糊匹配,最后返回默认
     */
    HttpServlet::SharedPtr getMatchedServlet(const String& uri);

private:
    typedef std::map<String, IServletCreator::UniquePtr> ServletMap;
    typedef std::pair<String, IServletCreator::UniquePtr> ServletPair;
    typedef std::vector<ServletPair> ServletVector;

private:
    ServletVector::iterator getGlobServletIter(StringArg uri);

private:
    std::shared_mutex mutex_;               ///< 读写互斥量
    ServletMap servlets_;                   ///< 精准匹配servlet MAP, uri(/Nemo/xxx) -> servlet
    ServletVector globServlets_;            ///< 模糊匹配servlet 数组, uri(/Nemo/*) -> servlet
    HttpServlet::SharedPtr defaultServlet_; ///< 默认servlet，所有路径都没匹配到时使用
};

/**
 * @brief NotFoundServlet(默认返回404)
 */
class NotFoundServlet : public HttpServlet {
public:
    typedef std::shared_ptr<NotFoundServlet> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<NotFoundServlet> UniquePtr; ///< 智能指针定义

public:
    NotFoundServlet();
    virtual int handle(HttpRequest* request, 
                    HttpResponse* response, 
                    HttpSession* session) override;

private:
    constexpr StringArg getNotFoundPage() const {
        return "<html>"
                "<head>"
                    "<title>"
                        "404 Not Found"
                    "</title>"
                "</head>"
                "<body>"
                    "<center>"
                        "<h1>404 Not Found</h1>"
                    "</center>"
                "</body>"
                "</html>";
    }

private:
    StringArg content_ = getNotFoundPage(); ///< 内容
};

} // namespace http
} // namespace net
} // namespace nemo