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