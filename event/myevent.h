/*  文件说明：
 *  1. 当主线程监听到事件时，根据事件类型构建一个特定类型的事件对象，加入线程池的待处理事件对象中等待被处理
 *  2. 线程池中会调用事件的 process 函数，函数中会根据对消息的处理状态执行对应的操作
 *  3. EventBase 表示所有事件的基类，其中包含两个 map 静态成员 requestStatus 和 responseStatus，使用套接字作为key，保存该套接字对应的请求消息(Request)或响应消息(Response)处理的状态
 *  4. AcceptConn 中的 process 函数用于接收新的连接并加入 epoll_wait 中
 *  5. HandleSig 中的 process 函数用于处理产生的各种事件
 *  6. 由于是静态成员，即使请求消息或响应消息没有接收完整或退出，下次产生事件时还会根据处理的状态继续执行下一步操作
 *  7. requestStatus 保存所有套接字当前对请求消息接收并处理了多少，根据请求消息的状态在 process 函数中对请求消息继续处理
 *  8. responseStatus 保存所有套接字当前对响应消息构建并发送了多少，根据请求消息的状态在 process 函数中对请求消息继续处理
 *  9. 如果某个套接字没有任何事件产生，requestStatus 和 responseStatus 中保存的事件会被清空
 */
#ifndef MYEVENT_H
#define MYEVENT_H
#include <dirent.h> //目录操作
#include <fstream>
#include <vector>
#include <cstdio>
#include <sys/stat.h>     //文件操作
#include <sys/socket.h>   //套接字
#include <arpa/inet.h>    //套接字
#include <sys/sendfile.h> //sendfile
#include <unistd.h>       //close

#include "../message/message.h"
#include "../utils/utils.h"

// 所有事件的基类
class EventBase
{
public:
    EventBase()
    {
    }
    virtual ~EventBase()
    {
    }

protected:
    // 保存文件描述符对应的请求的状态，因为一个连接上的数据可能非阻塞一次读取不完，所以保存到这里，当该连接上有新数据时，可以继续读取并处理
    static std::unordered_map<int, Request> requestStatus;

    // 保存文件描述符对应的发送数据的状态，一次process中非阻塞的写数据可能无法将数据全部传过去，所以保存当前数据发送的状态，可以继续传递数据
    static std::unordered_map<int, Response> responseStatus;

public:
    // 不同类型事件中重写该函数，以执行不同的处理方法
    virtual void process()
    {
    }
};

// 用于接受客户端连接的事件
class AcceptConn : public EventBase
{
public:
    AcceptConn(int listenFd, int epollFd) : m_listenFd(listenFd), m_epollFd(epollFd){};
    virtual ~AcceptConn(){};

public:
    virtual void process() override;

private:
    int m_listenFd; // 保存监听套接字
    int m_epollFd;  // 接收连接后加入的epoll
    int acceptFd;   // 保存接收到的连接套接字

    sockaddr_in clientAddr;  // 保存客户端地址
    socklen_t clientAddrLen; // 保存客户端地址长度
};

// 用于处理信号的事件，暂时没有处理信号事件需要，没有具体的实现
class HandleSig : public EventBase
{
public:
    HandleSig(int epollFd) : EventBase(){};
    virtual ~HandleSig(){};

public:
    virtual void process() override; // override表示重写基类的虚函数
};

// 处理客户端发送的请求
class HandleRecv : public EventBase
{
public:
    HandleRecv(int clientFd, int epollFd) : m_clientFd(clientFd), m_epollFd(epollFd){};
    virtual ~HandleRecv(){};

public:
    virtual void process() override;

private:
    int m_clientFd; // 客户端套接字，从该客户端读取数据
    int m_epollFd;  // epoll句柄，用于将新的连接加入epoll中
};
#endif