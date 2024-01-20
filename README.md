# cpp-httpclient
Wrapping the Asio library to create a simple HTTP client that supports connecting via domain names and IP addresses.

Mac and Linux is supported

# asio模块加载
```
git submodule update --init --recursive
```

# 编译
```
./build.sh
```

# 运行
```
Usage:
./cpp-httpclient [ip] [port] [method:GET/POST...] [url/path]
./cpp-httpclient --host [host] [method:GET/POST...] [url/path]
```

# 测试
借助在线服务（[https://httpbin.org/](https://httpbin.org/)）
```
./cpp-httpclient --host httpbin.org GET /get

connect httpbin.org [ip 184.73.216.86, port 80]

RESULT: HTTP/1.1 200  OK
response message:
{
  "args": {}, 
  "headers": {
    "Accept": "*/*", 
    "Host": "httpbin.org", 
    "X-Amzn-Trace-Id": "Root=1-65ab4f91-7370ba8a2d5dee213fb955d6"
  }, 
  "origin": "218.108.104.131", 
  "url": "http://httpbin.org/get"
}
```