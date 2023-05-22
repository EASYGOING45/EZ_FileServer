# README

## 基于Linux-C++的嵌入式Web文件服务器

## 开发者简介

- 姓名：李欢欢
- 学校：长安大学
- 院系：信息工程学院人工智能系
- 嵌入式系统大作业

## 技术栈

- 基于兰勇老师嵌入式课程、《TCP/IP网络编程》、《Linux高性能服务器编程》
- Linux-C++、C++11标准
- 简单HTML页面编写
- Linux Epoll核心编程、事件处理、线程池
- 操作系统进程间调度、多线程协程

## 主要功能

- 以HTML页面形式返回服务器指定目录文件夹下的所有文件
- 可以选择本地文件上传到服务器
- 所有文件显示在文件列表下
- 可以删除服务器中的指定文件

## 整体设计框架

- 服务器模型：使用`Reactor`事件处理模型，通过统一事件源，主线程使用`epoll`监听所有的事件，各个工作线程负责执行事件的逻辑处理
- 预先创建线程池，当有事件发生时，加入线程池的工作队列中，使用随机选择算法选择线程池中的一个 线程处理工作队列的事件
- 使用`HTTP-GET`方法获取文件列表，可以发起下载文件、删除文件的请求
- 使用`POST`方法向服务器上传文件，可以选择本地指定的文件
- 服务端使用`有限状态机`对请求消息进行解析，根据解析结果执行操作后，向客户端发送页面、发送文件或发送重定向报文
- 服务端使用`sendfile函数`实现零拷贝数据发送

![image-20230522081414629](https://happygoing.oss-cn-beijing.aliyuncs.com/img/image-20230522081414629.png)

## 模型讲解

![image-20230522083803154](https://happygoing.oss-cn-beijing.aliyuncs.com/img/image-20230522083803154.png)

### 线程池

线程池是一种实现并发执行的方式，在启动时会创建一定数量的线程，放在一个容器中。当客户端向服务端发起新的请求（事件）时，主线程（主调函数main）就会从线程池中取出一个线程来执行任务，任务执行完毕后，该线程并不退出，而是等待下一个任务的到来，以此实现线程的复用。

在文件服务器中，线程池起到了多线程处理客户端请求的作用。当有客户端请求时，可以将请求封装成一个个的任务，交由线程池中的某个线程处理，从而提高服务器的并发处理能力。同时，线程池可以控制线程数量，避免创建过多线程导致系统资源浪费和性能下降，对于需要大量读写文件或者网络通信的服务器程序，引进线程池模型可以有效提高服务器吞吐量和并发性以及响应速度。

```C++
/**
 * File threadpool.h
 * 1、用于创建线程池
 * 2、每个线程中等待事件队列中添加新事件（EventBase 指针指向的派生类）
 * 3、有新事件时，分配给一个线程处理，线程中调用事件的process()方法处理该事件
 */
static int tnum = 0;
class ThreadPool
{
public:
    // 初始化线程池、互斥访问事件队列的互斥量、表示队列中事件的信号量
    ThreadPool(int threadNum);
    ~ThreadPool();

public:
    // 向事件队列中添加一个待处理的事件，线程池中的线程会循环处理其中的事件
    int appendEvent(EventBase *event, const std::string eventType);

private:
    // 创建线程时指定的运行函数，参数传递this指针，实现在子线程中可以访问到该对象的成员
    static void *worker(void *arg);

    // 在线程中执行该函数等待处理事件队列中的事件
    void run();

private:
    int m_threadNum;      // 线程池中的线程个数
    pthread_t *m_threads; // 保存线程池中的所有线程

    std::queue<EventBase *> m_workQueue; // 事件队列 保存所有待处理的事件
    pthread_mutex_t queueLocker;         // 用于互斥访问事件队列的锁
    sem_t queueEventNum;                 // 表示队列中事件个数变化的信号量
};
#endif
```

### 主线程

```C++
#include "./fileserver/fileserver.h"

int main(void)
{
    WebServer server;

    // 创建线程池
    int ret = server.createThreadPool(4); // 四核线程池
    if (ret != 0)
    {
        std::cout << outHead("error") << "创建线程池失败" << std::endl;
        return -1;
    }

    // 初始化用于监听的套接字
    int port = 8888;
    ret = server.createListenFd(port);
    if (ret != 0)
    {
        std::cout << outHead("error") << "创建并初始化监听套接字失败" << std::endl;
        return -2;
    }

    // 初始化监听的epoll例程
    ret = server.createEpoll();
    if (ret != 0)
    {
        std::cout << outHead("error") << "初始化监听的epoll例程失败" << std::endl;
        return -3;
    }

    // 向epoll中添加监听套接字
    ret = server.epollAddListenFd();
    if (ret != 0)
    {
        std::cout << outHead("error") << "epoll添加监听套接字失败" << std::endl;
        return -4;
    }

    // 开启监听并处理请求
    ret = server.waitEpoll();
    if (ret != 0)
    {
        std::cout << outHead("error") << "epoll例程监听失败" << std::endl;
        return -5;
    }
    return 0;
}
```

主要包括以下基本流程：

1. 创建 WebServer 对象，用于管理服务器进程的配置和运行状态。
2. 调用 createThreadPool 函数创建一个 Threadpool 对象，指定线程池中线程的数量。
3. 调用 createListenFd 函数创建一个监听套接字，并初始化其相关参数，如端口号、IP 地址等。
4. 调用 createEpoll 函数创建一个 epoll 实例，并初始化其相关参数。
5. 调用 epollAddListenFd 函数将监听套接字添加到 epoll 实例中，开始监听客户端请求。
6. 调用 waitEpoll 函数进入监听循环，等待客户端请求并分配给工作线程池中的线程处理。

整个流程涉及了多个类的协作，包括 WebServer、ThreadPool、Epoll 等各种子类和基类。

其中，ThreadPool 用于维护线程池的状态和任务队列，Epoll 用于监听套接字事件，WebServer 则是管理服务器进程的核心类，负责协调整个流程的执行。

整个程序的运行过程是基于回调函数实现的，即由 Epoll 监听到客户端请求后，通过回调函数将工作交由指定的线程执行。

### Epoll

Epoll 是 Linux 内核提供的一种高级事件通知机制，用于 I/O 复用。它是目前 Linux 系统中最常用的 I/O 复用机制之一，也是实现高性能服务器应用的重要技术之一。

Epoll 模型的核心思想是基于事件驱动。程序需要先创建一个 Epoll 实例，并将需要监听的事件添加进去。当有事件发生时，Epoll 实例会触发事件，并用回调函数返回该事件的详细信息。基于这种机制，就可以实现高效的 I/O 复用，避免了频繁的轮询和阻塞，提高了程序的响应速度和吞吐量。

Epoll 模型主要包含以下三个核心 API：

1. epoll_create：创建一个 epoll 实例，返回一个文件描述符，用于后续的 epoll 操作。
2. epoll_ctl：对 epoll 实例进行管理，包括向 epoll 实例中添加、修改、删除监听的文件描述符和事件类型等。
3. epoll_wait：从 epoll 实例中等待事件发生，并返回发生事件的文件描述符和事件类型。

Epoll 模型与其他 I/O 复用技术相比，具有以下优势：

1. 支持大规模并发连接：Epoll 模型可以支持大量的并发连接，因为它是基于事件驱动的机制，而不是基于线程或进程的机制。只需要在 Epoll 实例中添加需要监听的文件描述符，就可以实现并发连接高效处理。
2. 高效的 I/O 复用：Epoll 模型采用了回调函数的方式，避免了频繁的轮询和阻塞，提高了程序的响应速度和吞吐量。
3. 更好的扩展性：Epoll 可以监听多个文件描述符，支持 ET（边沿触发）和 LT（水平触发）等不同的工作模式，更加灵活。

```C++
#ifndef FILESERVER_H
#define FILESERVER_H
#include <signal.h>
#include <sys/types.h>
#include <stdexcept>
#include <errno.h>

#include "../threadpool/threadpool.h"

#define MAX_RESEVENT_SIZE 1024 // 事件的最大个数

class WebServer
{
public:
    WebServer();
    ~WebServer();

    // 创建套接字等待客户端连接，并开启监听
    int createListenFd(int port, const char *ip = nullptr);

    // 创建epoll例程用于监听套接字
    int createEpoll();

    // 向epoll中添加监控Listen 套接字
    int epollAddListenFd();

    // 设置监听事件处理的管道
    int epollAddEventPipe();

    // 设置term和alarm信号的处理
    int addHandleSig(int signo = -1);

    // 信号处理函数
    static void setSigHandler(int signo);

    // 主线程中负责监听所有事件
    int waitEpoll();

    // 创建线程池
    int createThreadPool(int threadNum = 8); // 默认8线程

private:
    int m_listenfd;           // 服务端的套接字
    sockaddr_in m_serverAddr; // 服务端套接字绑定的地址信息
    static int m_epollfd;     // I/O复用的epoll例程未见描述符
    static bool isStop;       // 是否暂停服务器

    static int eventHandlerPipe[2]; // 用于统一事件源传递信号的管道

    epoll_event resEvents[MAX_RESEVENT_SIZE]; // 保存 epoll_wait 结果的数组

    ThreadPool *threadPool;
};
#endif
```



## 运行部署

- 操作系统：ARM-Linux操作系统（Redhat、Centos、Ubuntu等均可）、
- 编译器及语言要求：g++7.0及以上、C++11
- 运行
- build项目 ：`sh ./build.sh`
- 启动服务器：`./main`
- 客户端启动：浏览器输入地址:`serverIP:8888` serverIP为服务器IP地址