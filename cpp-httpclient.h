#pragma once

#include "asio.hpp"
#include <memory>
#include <string>
#include <future>
#include <map>
#include <ostream>
#include <thread>
#include <memory>
#include <iostream>
#include <stdio.h>

namespace cpphttp
{

////HTTP客户端定义
class HttpClient : public std::enable_shared_from_this<HttpClient>
{
public:
    struct Response
    {
        std::string                         http_version;
        unsigned int                        status_code = 0;
        std::string                         status_message;
        std::map<std::string, std::string>  headers;
        std::string                         body;
        std::string                         error;
    };
    
    // 1. arg is host(string);
    // 2. args are ip(string) and port(uint16_t);
    template<typename... Args>
    static std::shared_ptr<HttpClient> Create(Args&&... args);
    // static std::shared_ptr<HttpClient> Create(const std::string &host);
    // static std::shared_ptr<HttpClient> Create(const std::string &ip, uint16_t port);

    Response Request(const std::string &method, /***GET/POST/...*/
                    const std::string &url,     /***path...******/
                    const std::map<std::string, std::string> &header = std::map<std::string, std::string>(),
                    const std::string &body = "");
    
    ~HttpClient();

private:
    HttpClient(const std::string &ip, uint16_t port);
    HttpClient(const std::string &host);
    void start();
    void stop();
    void async_resolve(void);
    void async_connect(void);
    void async_send(const std::string &method, 
                    const std::string &url, 
                    const std::map<std::string, std::string> &header, 
                    const std::string &body);
    void async_recv_status_line(void);
    void async_recv_headers(void);
    void async_recv_content(void);
    void close(void);
    void init(void);
    void set_pro(const std::string &error);

private:
    std::thread processor;
    asio::io_context ioctx;
    asio::executor_work_guard<asio::io_context::executor_type> work;
    bool need_resolve_ = false;
    asio::ip::tcp::resolver resolver_;
    asio::ip::tcp::socket socket;
    std::string host;
    std::promise<std::string> pro;
    asio::ip::tcp::endpoint endpoint; // ip+port
    asio::ip::tcp::resolver::results_type endpoints_; // dns
    std::shared_ptr<asio::streambuf> request;
    std::shared_ptr<asio::streambuf> response;
    std::unique_ptr<Response> http_response;
};

////HTTP客户端实现
HttpClient::HttpClient(const std::string &ip, uint16_t port)
: work(ioctx.get_executor())
, need_resolve_(false)
, resolver_(ioctx)
, socket(ioctx)
, host(ip + ":" + std::to_string(port))
, endpoint(asio::ip::make_address_v4(ip), port)
{
    start();
}

HttpClient::HttpClient(const std::string &host_)
: work(ioctx.get_executor())
, need_resolve_(true)
, resolver_(ioctx)
, socket(ioctx)
, host(host_)
{
    start();
}

HttpClient::~HttpClient()
{
    stop();
}

void HttpClient::start()
{
    processor = std::thread([this](){
        ioctx.run();
    });
}

void HttpClient::stop()
{
    ioctx.stop();
    if (processor.joinable()) {
        processor.join();
    }
}

template<typename... Args>
std::shared_ptr<HttpClient> HttpClient::Create(Args&&... args)
{
    return std::shared_ptr<HttpClient>(new HttpClient(std::forward<Args>(args)...));
}
// std::shared_ptr<HttpClient> HttpClient::Create(const std::string &host)
// {
//     return std::shared_ptr<HttpClient>(new HttpClient(host));
// }
// std::shared_ptr<HttpClient> HttpClient::Create(const std::string &ip, uint16_t port)
// {
//     return std::shared_ptr<HttpClient>(new HttpClient(ip, port));
// }

void HttpClient::close(void)
{
    asio::error_code ec;
    socket.close(ec);
}

void HttpClient::init(void)
{
    request = std::make_shared<asio::streambuf>();
    response = std::make_shared<asio::streambuf>();
    http_response.reset(new Response());
}

void HttpClient::set_pro(const std::string &error)
{
	try {
		pro.set_value(error);
	} catch (...) {}
}

void HttpClient::async_send(const std::string &method,
                            const std::string &url,
                            const std::map<std::string, std::string> &header, 
                            const std::string &body)
{
    std::ostream request_stream(request.get());
    // pack request data.
    request_stream << method << " " << url << " " << "HTTP/1.1\r\n";
    for (auto &s : header) {
        request_stream << s.first << ": " << s.second << "\r\n";
    }
    if (!body.empty()) {
        request_stream << "Content-Length: " << body.size() << "\r\n";
    }
    request_stream << "Host: " << host << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";
    request_stream << body;
    // Send the request.
    pro = std::promise<std::string>();
    auto self(this->shared_from_this());
    auto callback = [self, this](const asio::error_code &ec, std::size_t bytes_transferred) {
        if (!ec) {
            async_recv_status_line();
        } else {
            set_pro(ec.message());
        }
    };
    asio::async_write(socket, *request, callback);
}

void HttpClient::async_recv_status_line()
{
    auto self(this->shared_from_this());
    auto callback = [self, this](const asio::error_code& ec, std::size_t size) {
        if (!ec) {
            std::istream response_stream(response.get());
            response_stream >> http_response->http_version;
            response_stream >> http_response->status_code;
            std::getline(response_stream, http_response->status_message);
            if (!response_stream || http_response->http_version.substr(0, 5) != "HTTP/") {
                set_pro("invalid http response");
            } else {
                async_recv_headers();
            }
        } else {
            set_pro(ec.message());
        }
    };
    asio::async_read_until(socket, *response, "\r\n", callback);
}

void HttpClient::async_recv_headers()
{
    auto self(this->shared_from_this());
    auto callback = [self, this](const asio::error_code& ec, std::size_t size) {
       if (!ec) {
            std::istream response_stream(response.get());
            std::string header;
            while (std::getline(response_stream, header) && header != "\r") {
                std::size_t found = header.find(":");
                if (found != std::string::npos) {
                    http_response->headers.emplace(header.substr(0, found), header.substr(found + 2, header.size() - found - 3));
                }
            }
            async_recv_content();
        } else {
            set_pro(ec.message());
        }
    };
    asio::async_read_until(socket, *response, "\r\n\r\n", callback);
}

void HttpClient::async_recv_content()
{
    auto self(this->shared_from_this());
    auto callback = [self, this](const asio::error_code& ec, std::size_t size) {
        if (!ec) {
            async_recv_content();
        } else {
            if (ec != asio::error::eof) {
                set_pro(ec.message());
                return;
            }
            http_response->body = std::string(asio::buffers_begin(response->data()), asio::buffers_end(response->data()));
            set_pro("");
        }
    };
    asio::async_read(socket, *response, asio::transfer_at_least(1), callback);
}

void HttpClient::async_resolve(void)
{
    pro = std::promise<std::string>();
    if (need_resolve_) {
        auto self(this->shared_from_this());
        auto callback = [self, this](const asio::error_code& ec, const asio::ip::tcp::resolver::results_type endpoints) {
            if (!ec) {
                set_pro("");
                endpoints_ = endpoints;
            } else {
                set_pro(ec.message());
            }
        };
        resolver_.async_resolve(host, "http", callback);
    } else {
        set_pro("");
    }
}

void HttpClient::async_connect(void)
{
    pro = std::promise<std::string>();
    auto self(this->shared_from_this());
    if (need_resolve_) {
        auto callback = [self, this](const asio::error_code& ec, const asio::ip::tcp::endpoint &endpoint) {
            if (!ec) {
                std::cout << "connect " << host << " [ip " << endpoint.address().to_string() << 
                            ", port " <<  endpoint.port() << "]" << std::endl;
                set_pro("");
            } else {
                set_pro(ec.message());
            }
        };
        asio::async_connect(socket, endpoints_, callback);
    } else {
        auto callback = [self, this](const asio::error_code& ec) {
            if (!ec) {
                set_pro("");
            } else {
                set_pro(ec.message());
            }
        };
        socket.async_connect(endpoint, callback);
    }
}

HttpClient::Response HttpClient::Request(const std::string &method,
                                         const std::string &url,
                                         const std::map<std::string, std::string> &header, 
                                         const std::string &body/* = "" */)
{
    try {
        //初始化
        init();
        auto connectTimeoutSecond = std::chrono::seconds(3);
        auto requestTimeoutSecond = std::chrono::seconds(30);

        // dns 解析
        async_resolve();
        std::future<std::string> fu = pro.get_future();
        if (fu.valid()) {
            std::future_status status = fu.wait_for(connectTimeoutSecond);
            if (status == std::future_status::ready) {
                http_response->error = fu.get();
            } else {
                http_response->error = "resolve timeout";
            }
        } else {
            http_response->error = "resolve future invalid";
        }

        // 连接主机
        if (http_response->error.empty()) {
            async_connect();
            std::future<std::string> fu = pro.get_future();
            if (fu.valid()) {
                std::future_status status = fu.wait_for(connectTimeoutSecond);
                if (status == std::future_status::ready) {
                    http_response->error = fu.get();
                } else {
                    http_response->error = "connect timeout";
                }
            } else {
                http_response->error = "connect future invalid";
            }
        }

        //发送请求
        if (http_response->error.empty()) {
            async_send(method, url, header, body);
            std::future<std::string> fu = pro.get_future();
            if (fu.valid()) {
                std::future_status status = fu.wait_for(requestTimeoutSecond);
                if (status == std::future_status::ready) {
                    http_response->error = fu.get();
                } else {
                    http_response->error = "request timeout";
                }
            } else {
                http_response->error = "request future invalid";
            }
        }
    } catch (...) {
        http_response->error = "request occured exception";
    }
    std::cout << http_response->error << std::endl;
    //关闭连接
    close();
    //返回响应结果
    return *http_response;
}

} // end of namespace cpphttp