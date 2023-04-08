#include "net/http/servlet.h"

#include "net/http/http.h"

namespace nemo {
namespace net {
namespace http {

HttpServlet::HttpServlet(StringArg name) :
    name_(name) {
}

FunctionServlet::FunctionServlet(const Callback& cb) :
    HttpServlet("FunctionServlet"),
    cb_(cb) {
}

FunctionServlet::FunctionServlet(Callback&& cb) :
    HttpServlet("FunctionServlet"),
    cb_(std::move(cb)) {
}

int FunctionServlet::handle(HttpRequest* request, 
        HttpResponse* response, 
        HttpSession* session) {
    return cb_(request, response, session);
}

HoldServletCreator::HoldServletCreator(const HttpServlet::SharedPtr& servlet) :
    servlet_(servlet) {
}

ServletDispatcher::ServletVector::iterator 
ServletDispatcher::getGlobServletIter(StringArg uri) {
    return std::find_if(globServlets_.begin(), 
        globServlets_.end(), 
        [&uri](const ServletPair& servletPair) {
            return uri == servletPair.second->getName();
    });
}

ServletDispatcher::ServletDispatcher() :
    HttpServlet("ServletDispatcher"),
    defaultServlet_(new NotFoundServlet) {
}

int ServletDispatcher::handle(HttpRequest* request, 
                HttpResponse* response, 
                HttpSession* session) {
    HttpServlet::SharedPtr servlet = getMatchedServlet(request->getPath());
    if(servlet) {
        servlet->handle(request, response, session);
        return 0;
    }
    return -1;
}

void ServletDispatcher::addServlet(StringArg uri, HttpServlet::UniquePtr&& servlet) {
    std::lock_guard<std::shared_mutex> lockGuard(mutex_);
    servlets_.emplace(String(uri), IServletCreator::UniquePtr(new 
        HoldServletCreator(std::move(servlet))));
}

void ServletDispatcher::addServlet(StringArg uri, const FunctionServlet::Callback& cb) {
    std::lock_guard<std::shared_mutex> lockGuard(mutex_);
    HttpServlet::SharedPtr servlet(new FunctionServlet(cb));
    servlets_.emplace(String(uri), IServletCreator::UniquePtr(new HoldServletCreator(servlet)));
}

void ServletDispatcher::addServlet(StringArg uri, FunctionServlet::Callback&& cb) {
    std::lock_guard<std::shared_mutex> lockGuard(mutex_);
    HttpServlet::SharedPtr servlet(new FunctionServlet(std::move(cb)));
    servlets_.emplace(String(uri), IServletCreator::UniquePtr(new HoldServletCreator(servlet)));
}

void ServletDispatcher::addGlobServlet(StringArg uri, HttpServlet::UniquePtr&& servlet) {
    std::lock_guard<std::shared_mutex> lockGuard(mutex_);
    auto iter = getGlobServletIter(uri);
    if (iter != globServlets_.end()) {
        iter->second.reset(new HoldServletCreator(std::move(servlet)));
    } else {
        globServlets_.emplace_back(String(uri), 
            IServletCreator::UniquePtr(new HoldServletCreator(std::move(servlet))));
    }
}

void ServletDispatcher::addGlobServlet(StringArg uri, const FunctionServlet::Callback& cb) {
    addGlobServlet(uri, HttpServlet::UniquePtr(new FunctionServlet(cb)));
}

void ServletDispatcher::addGlobServlet(StringArg uri, FunctionServlet::Callback&& cb) {
    addGlobServlet(uri, HttpServlet::UniquePtr(new FunctionServlet(std::move(cb))));
}

void ServletDispatcher::addServletCreator(StringArg uri, IServletCreator::UniquePtr&& creator) {
    std::lock_guard<std::shared_mutex> lockGuard(mutex_);
    servlets_.emplace(String(uri), std::move(creator));
}

void ServletDispatcher::addGlobServletCreator(StringArg uri, IServletCreator::UniquePtr&& creator) {
    std::lock_guard<std::shared_mutex> lockGuard(mutex_);
    auto iter = getGlobServletIter(uri);
    if (iter != globServlets_.end()) {
        iter->second = std::move(creator);
    } else {
        globServlets_.emplace_back(String(uri), std::move(creator));
    }
}

void ServletDispatcher::delServlet(const String& uri) {
    std::lock_guard<std::shared_mutex> lockGuard(mutex_);
    servlets_.erase(uri);
}

void ServletDispatcher::delGlobServlet(StringArg uri) {
    std::lock_guard<std::shared_mutex> lockGuard(mutex_);
    auto iter = getGlobServletIter(uri);
    if (iter != globServlets_.end()) {
        globServlets_.erase(iter);
    }
}

HttpServlet::SharedPtr ServletDispatcher::getServlet(const String& uri) {
    std::shared_lock<std::shared_mutex> sharedLock(mutex_);
    auto iter = servlets_.find(uri);
    return iter == servlets_.end() ? nullptr : iter->second->get();
}

HttpServlet::SharedPtr ServletDispatcher::getGlobServlet(const String& uri) {
    std::shared_lock<std::shared_mutex> sharedLock(mutex_);
    auto iter = getGlobServletIter(uri);
    return iter == globServlets_.end() ? nullptr : iter->second->get();
}

HttpServlet::SharedPtr ServletDispatcher::getMatchedServlet(const String& uri) {
    std::shared_lock<std::shared_mutex> sharedLock(mutex_);
    HttpServlet::SharedPtr result = getServlet(uri);
    if (nullptr == result) {
        result = getGlobServlet(uri);
    }
    if (nullptr == result) {
        result = defaultServlet_;
    }
    return result;
}

NotFoundServlet::NotFoundServlet() : 
    HttpServlet("NotFoundServlet") {
}

int NotFoundServlet::handle(HttpRequest* request, 
                    HttpResponse* response, 
                    HttpSession* session) {
    response->setStatus(HttpStatus::NOT_FOUND);
    response->setHeader("Server", "nemo/1.0.0");
    response->setHeader("Content-Type", "text/html");
    response->setBody(content_);
    return 0;
}

} // namespace http
} // namespace net
} // namespace nemo