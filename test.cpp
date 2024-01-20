#include "cpp-httpclient.h"
#include <iostream>
#include <sstream>
#include <string>

using namespace cpphttp;

int main(int argc, char **argv)
{
    if (argc != 5) {
        std::ostringstream oss;
        oss << "Usage:\n" << argv[0] << " [ip] [port] [method:GET/POST...] [url/path]";
        oss << "\n" << argv[0] << " --host [host] [method:GET/POST...] [url/path]\n";
        std::cerr << oss.str() << std::endl;
        exit(1);
    }
    std::string host;
    std::string ip;
    uint16_t port;
    std::string method;
    std::string url;
    if (strcmp(argv[1], "--host") == 0) {
        host = argv[2];
    } else {
        ip = argv[1];
        port = std::stoi(argv[2]);
    }
    method = argv[3];
    url = argv[4];
    std::shared_ptr<HttpClient> httpClient = nullptr;
    if (!host.empty()) {
        httpClient = HttpClient::Create(host);
    } else {
        httpClient = HttpClient::Create(ip, port);
    }

    HttpClient::Response rep = httpClient->Request(method, url);
    
    std::cout << "RESULT: " << rep.http_version << " " 
                            << rep.status_code << " " 
                            << rep.status_message << " "
                             << rep.error << std::endl;
    std::cout << "response message:\n" << rep.body << std::endl;
    
    return 0;
}