#include "myevent.h"

// 类外初始化静态成员
std::unordered_map<int, Request> EventBase::requestStatus;   // 保存文件描述符对应的请求的状态
std::unordered_map<int, Response> EventBase::responseStatus; // 保存文件描述符对应的发送数据的状态

void AcceptConn::process()
{
    // 接受连接
    clientAddrLen = sizeof(clientAddr);
    int accFd = accept(m_listenFd, (sockaddr *)&clientAddr, &clientAddrLen);
    if (accFd == -1)
    {
        std::cout << outHead("error") << "接受新连接失败" << std::endl;
        return;
    }
    // 将连接设置为非阻塞
    setNonBlocking(accFd);

    // 将连接加入到监听，客户端套接字都设置为 EPOLLET 和 EPOLLONESHOT
    addWaitFd(m_epollFd, accFd, true, true); // 将连接加入到监听
    std::cout << outHead("info") << "接受新连接" << accFd << "成功！" << std::endl;
}

void HandleRecv::process()
{
    std::cout << outHead("info") << "开始处理客户端" << m_clientFd << " 的一个HandleRecv事件" << std::endl;
    // 获取Request对象，保存到m_clientFd索引的requestStatus中，没有的话会自动创建一个新的
    requestStatus[m_clientFd];

    // 读取输入，检测是否断开连接，否则处理请求
    char buf[2048];
    int recvLen = 0;

    while (1)
    {
        // 循环接收数据，直到缓冲区读取不到数据或请求消息处理完成时退出循环
        recvLen = recv(m_clientFd, buf, 2048, 0);

        // 对方关闭连接，直接断开连接，设置当前状态为 HANDLE_ERROR 再退出循环
        if (recvLen == 0)
        {
            std::cout << outHead("info") << "客户端" << m_clientFd << "断开连接" << std::endl;
            requestStatus[m_clientFd].status = HANDLE_ERROR;
            break;
        }

        // 如果缓冲区的数据已经读完，退出读数据的状态
        if (recvLen == -1)
        {
            if (errno != EAGAIN)
            {
                // 如果不是缓冲区为空，设置状态为错误，并退出循环
                requestStatus[m_clientFd].status = HANDLE_ERROR;
                std::cout << outHead("error") << "接收数据时返回 -1 (errno = " << errno << ")" << std::endl;
                break;
            }
            // 如果是缓冲区为空，表示需要等待数据发送，由于是EPOLLONESHOT，再退出循环，等再发来数据时再来处理
            modifyWaitFd(m_epollFd, m_clientFd, true, true, false);
            break;
        }
    }
}