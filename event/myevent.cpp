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

        // 将收到的数据拼接到之前接收到的数据的后面，由于在处理文件时，里面可能有 \0 所以使用append将buf内的所有字符都保存到recvMsg中
        requestStatus[m_clientFd].recvMsg.append(buf, recvLen);

        // 边接收数据边处理
        // 根据请求报文的状态执行操作，以下操作中，如果成功了，则解析请求报文的下个部分，如果某个部分还没有完全接收，会退出当前处理步骤，等再次收到数据后根据这次解析的状态继续处理

        // 保存字符串查找结果，每次查找都可以用该变量暂存查找结果
        std::string::size_type endIndex = 0;

        // 如果是初始状态，获取请求行
        if (requestStatus[m_clientFd].status == HANDLE_INIT)
        {
            endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n"); // 查找请求行的结束边界
            if (endIndex != std::string::npos)                         // npos是string::size_type的一个静态成员常量，值为-1，表示未找到子串
            {
                // 保存请求行
                requestStatus[m_clientFd].setRequestLine(requestStatus[m_clientFd].recvMsg.substr(0, endIndex + 2)); // std::cout << requestStatus[m_clientFd].recvMsg.substr(0, endIndex + 2);
                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2);                                            // 删除收到的数据中的请求行
                requestStatus[m_clientFd].status = HANDLE_HEAD;                                                      // 将状态设置为处理消息首部
                std::cout << outHead("info") << "处理客户端" << m_clientFd << "的请求行完成" << std::endl;
            }

            // 如果没有找到 \r\n，表示数据还没有接收完成，会跳回上面继续接收数据
        }

        // 如果是处理首部的状态，逐行解析首部字段，直至遇到空行
        if (requestStatus[m_clientFd].status == HANDLE_HEAD)
        {
            std::string curLine; // 用于暂存获取的一行数据
            while (1)
            {
                endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n"); // 获取一行的边界
                if (endIndex == std::string::npos)
                {
                    // 如果没有找到边界，表示后面的数据还没有接收完整，退出循环，等待下次接收后处理
                    break;
                }

                curLine = requestStatus[m_clientFd].recvMsg.substr(0, endIndex + 2); // 将该行的内容取出
                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2);            // 删除收到的数据中的该行数据
            }
        }
    }
}