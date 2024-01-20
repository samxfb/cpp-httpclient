#!/bin/bash

g++ -std=c++11 -I. -I./thirdparty/asio/asio/include -pthread -o cpp-httpclient test.cpp
