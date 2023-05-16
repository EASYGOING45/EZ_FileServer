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

                if (curLine == "\r\n")
                {
                    requestStatus[m_clientFd].status = HANDLE_BODY; // 如果是空行，将状态修改为等待解析消息体
                    if (requestStatus[m_clientFd].msgHeader["Content-Type"] == "multipart/form-data")
                    {
                        // 如果接收的是文件，设置消息体中文件的处理状态
                        requestStatus[m_clientFd].fileMsgStatus = FILE_BEGIN_FLAG;
                    }
                    std::cout << outHead("info") << "处理客户端" << m_clientFd << "的消息首部完成" << std::endl;
                    if (requestStatus[m_clientFd].requestMethod == "POST")
                    {
                        std::cout << outHead("info") << "客户端" << m_clientFd << "发送POST请求，开始处理请求体" << std::endl;
                    }
                    break;
                }
                requestStatus[m_clientFd].addHeaderOpt(curLine); // 如果不是空行，需要将该首部保存到map中
            }
        }

        // 如果是处理消息体的状态，根据请求类型执行特定的操作
        if (requestStatus[m_clientFd].status == HANDLE_BODY)
        {
            // GET 操作时表示请求数据，将请求的资源路径交给 HandleSend事件处理
            if (requestStatus[m_clientFd].requestMethod == "GET")
            {
                // 设置响应消息的资源路径，在HandleSend中根据请求资源构建整个响应消息并发送
                responseStatus[m_clientFd].bodyFileName = requestStatus[m_clientFd].requestResource;

                // 设置监听套接字的可写事件，当套接字写缓冲区有空闲数据时，会产生 HandleSend 事件，将 m_clientFd 索引的 responseStatus 中的数据发送
                modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                requestStatus[m_clientFd].status = HANDLE_COMPLATE;
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 发送 GET 请求，已将请求资源构成 Response 写事件等待发送数据" << std::endl;
                break;
            }

            // TAG
            // 处理客户端发送的POST请求体中的文件信息
            //  POST方法表示上传数据，执行接收数据相关的操作
            if (requestStatus[m_clientFd].requestMethod == "POST")
            {
                // 记录未处理的数据长度，用于当前if步骤处理结束时，计算处理了多少消息体数据
                // 处理非文件时用来判断数据边界（文件使用boundary确定边界）
                std::string::size_type beginSize = requestStatus[m_clientFd].recvMsg.size();
                if (requestStatus[m_clientFd].msgHeader["Content-Type"] == "multipart/form-data")
                { // 如果发送的是文件
                    // 如果处于等待处理文件凯斯标志的状态，查找\r\n判断标志部分是否已经接收
                    if (requestStatus[m_clientFd].fileMsgStatus == FILE_BEGIN_FLAG)
                    {
                        std::cout << outHead("info") << "客户端" << m_clientFd << " 的POST请求用于上传文件，正在寻找文件头开始边界..." << std::endl;
                        // 查找\r\n边界
                        endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n");

                        // 当前状态下 \r\n前的数据必然是文件信息开始的标志
                        if (endIndex != std::string::npos)
                        {
                            // string::npos是string::size_type的一个静态成员常量，值为-1，表示未找到子串
                            // 找到了边界，将边界前的数据保存到文件信息开始标志中
                            std::string flagStr = requestStatus[m_clientFd].recvMsg.substr(0, endIndex);

                            if (flagStr == "--" + requestStatus[m_clientFd].msgHeader["boundary"])
                            {                                                             // 如果等于"--" + boundary，表示找到了文件信息开始的标志 则进入下一个状态
                                requestStatus[m_clientFd].fileMsgStatus = FILE_HEAD;      // 进入下一个状态
                                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2); // 将开始标志行删除（包括\r\n）
                                std::cout << outHead("info") << "客户端" << m_clientFd << "的POST请求体中找到文件头开始边界，正在处理文件头..." << std::endl;
                            }
                            else
                            {
                                // 如果和边界不同，表示出错，直接返回重定向报文，重新请求文件列表
                                responseStatus[m_clientFd].bodyFileName = '/redirect';
                                modifyWaitFd(m_epollFd, m_clientFd, true, true, true); // 重置可读事件和可写事件，用于发送重定向回复报文
                                requestStatus[m_clientFd].status = HANDLE_COMPLATE;    // 设置状态为处理完成
                                std::cout << outHead("error") << "客户端 " << m_clientFd << " 的 POST 请求体中没有找到文件头开始边界，添加重定向 Response 写事件，使客户端重定向到文件列表" << std::endl;
                                break;
                            }
                        }
                    }

                    // FLAG
                    // 如果处于等待接收并处理消息体中文件头部信息的状态，从中提取文件名
                    if (requestStatus[m_clientFd].fileMsgStatus == FILE_HEAD)
                    {
                        std::string strLine;
                        while (1)
                        {
                            // 查找 \r\n表示一行数据
                            endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n");
                            if (endIndex != std::string::npos)
                            {
                                strLine = requestStatus[m_clientFd].recvMsg.substr(0, endIndex + 2); // 获取这一行的数据信息
                                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2);            // 删除这一行的数据

                                // 检测是否为空行，如果是空行，修改状态，退出
                                if (strLine == "\r\n")
                                {
                                    requestStatus[m_clientFd].fileMsgStatus = FILE_CONTENT;
                                    std::cout << outHead("info") << "客户端" << m_clientFd << " 的POST请求体中文件头处理成功，正在接收并保存文件内容..." << std::endl;
                                    break;
                                }
                                // 查找strLine中是否包含filename
                                endIndex = strLine.find("filename");
                                if (endIndex != std::string::npos)
                                {
                                    strLine.erase(0, endIndex + std::string("filename=\"").size()); // 将真正fiename前的所有字符删除
                                    for (int i = 0; strLine[i] != '\"'; ++i)
                                    {
                                        // 保存文件名
                                        requestStatus[m_clientFd].recvFileName += strLine[i];
                                    }
                                    std::cout << outHead("info") << "客户端" << m_clientFd << " 的POST请求体中找到文件名字" << requestStatus[m_clientFd].recvFileName << " , 继续处理文件头..." << std::endl;
                                }
                            }
                            else
                            {
                                // 如果没有找到，表示消息还没有完整接收，退出，等待下一轮的事件中继续处理
                                break;
                            }
                        }
                    }

                    // FLAG-2023-0516
                    // 如果处于等待并处理消息体中文件内容部分
                    // 循环检索是否有 \r\n,将 \r\n之前的内容全部保存
                    // 如果存在\r\n，根据后面的内容判断是否到达文件边界
                    if (requestStatus[m_clientFd].fileMsgStatus == FILE_CONTENT)
                    {
                        // 首先以二进制追加的方式打开文件
                        std::ofstream ofs("filedir/" + requestStatus[m_clientFd].recvFileName, std::ios::out | std::ios::app | std::ios::binary);
                        if (!ofs)
                        {
                            std::cout << outHead("error") << "客户端" << m_clientFd << " 的POST 请求体所需要保存的文件打开失败，正在重新打开文件..." << std::endl;
                            break;
                        }
                        while (1)
                        {
                            int saveLen = requestStatus[m_clientFd].recvMsg.size(); // 用于存储 根据\r的位置决定向文件中写入多少字符，初始为所有字符长度
                            if (saveLen == 0)
                            {
                                // 长度为空时退出循环，等待接收到数据时再处理
                                break;
                            }
                            // 在剩余的字符中搜索标志 \r
                            endIndex = requestStatus[m_clientFd].recvMsg.find('\r');

                            if (endIndex != std::string::npos)
                            {
                                // 如果有 \r,后面有可能是文件结束标识
                                // 首先判断\r后的数据是否满足结束标识的长度，是否大于等于sizeof(\r\n+"--"+boundary+"--"+\r\n)
                                int boundarySecLen = requestStatus[m_clientFd].msgHeader["boundary"].size() + 8;
                                if (requestStatus[m_clientFd].recvMsg.size() - endIndex >= boundarySecLen)
                                {
                                    // 判断后面这部分数据是否为结束边界 \r\n
                                    if (requestStatus[m_clientFd].recvMsg.substr(endIndex, boundarySecLen) ==
                                        "\r\n--" + requestStatus[m_clientFd].msgHeader["boundary"] + "--\r\n")
                                    {
                                        if (endIndex == 0)
                                        {
                                            // 表示边界前的数据都已经写入文件，设置文件接收完成，进入下一个状态
                                            std::cout << outHead("info") << "客户端" << m_clientFd << " 的POST请求体中的文件数据接受并保存完毕" << std::endl;
                                            requestStatus[m_clientFd].fileMsgStatus = FILE_COMPLATE; // 设置文件接收完成
                                            break;
                                        }

                                        // 如果后面不是结束标识，先将\r 之前的所有数据写入文件，在循环的下一轮会进到上一个if，结束整个处理过程
                                        saveLen = endIndex
                                    }
                                    else
                                    {
                                        // 如果不是边界，在\r后再次搜索\r，如果搜索到了，写入的数据截至到第二个\r，否则将所有数据写入
                                        endIndex = requestStatus[m_clientFd].recvMsg.find('\r', endIndex + 1);
                                        if (endIndex != std::string::npos)
                                        {
                                            saveLen = endIndex;
                                        }
                                    }
                                }
                                else
                                {
                                    // 如果后面的数据长度还不够，则将/r之前的数据写入文件，并等待接收后面的数据

                                    // 如果/r之前的数据已经写入文件，退出循环，等待接收更多数据后再进入该循环
                                    if (endIndex == 0)
                                    {
                                        break;
                                    }

                                    // 否则将endIndex前的数据写入文件
                                    saveLen = endIndex;
                                }
                            }
                            // 如果没有退出表示当前仍是数据部分，将saveLen字节的数据存入文件，并将这些数据从recvMsg数据中删除
                            ofs.write(requestStatus[m_clientFd].recvMsg.c_str(), saveLen);
                            requestStatus[m_clientFd].recvMsg.erase(0, saveLen);
                        }
                        ofs.close();
                    }
                    // std::cout<<"已经退出文件接收函数"<<std::endl;
                    // 如果文件已经处理完成，设置消息体为完成状态
                    if (requestStatus[m_clientFd].fileMsgStatus == FILE_COMPLATE)
                    {
                        // 设置响应消息的资源路径，再HandleSend中根据请求资源构建整个响应消息并发送
                        responseStatus[m_clientFd].bodyFileName = "/redirect";
                        modifyWaitFd(m_epollFd, m_clientFd, true, true, true); // 重置可读事件和可写事件，用于发送重定向回复报文
                        requestStatus[m_clientFd].status = HANDLE_COMPLATE;
                        std::cout << outHead("info") << "客户端" << m_clientFd << " 的POST请求体处理完成，添加Response写事件，发送重定向报文刷新文件列表" << std::endl;
                        break;
                    }
                }
                else
                {
                    // POST是其他类型的数据
                    // 其他POST类型的数据时，直接返回重定向报文，获取文件列表
                    responseStatus[m_clientFd].bodyFileName = "/redirect";
                    modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                    requestStatus[m_clientFd].status = HANDLE_COMPLATE;
                    std::cout << outHead("error") << "客户端" << m_clientFd << " 的POST请求中接收到不能处理的数据，添加Response写事件，返回重定向到文件列表的报文" << std::endl;
                    break;
                }
            }
        }
    }

    if (requestStatus[m_clientFd].status == HANDLE_COMPLATE)
    {
        // 如果请求处理完成，将该套接字对应的请求删除
        std::cout << outHead("info") << "客户端" << m_clientFd << " 的请求消息处理成功" << std::endl;
        requestStatus.erase(m_clientFd);
    }
    else if (requestStatus[m_clientFd].status == HANDLE_ERROR)
    {
        // 请求处理错误，关闭该文件描述符，将该套接字对应的请求删除，从监听列表中删除该文件描述符
        std::cout << outHead("error") << "客户端" << m_clientFd << " 的请求消息处理失败，关闭连接" << std::endl;
        // 先删除监听的文件描述符
        deleteWaitFd(m_epollFd, m_clientFd);
        // 再关闭文件描述符
        shutdown(m_clientFd, SHUT_RDWR);
        close(m_clientFd);
        requestStatus.erase(m_clientFd);
    }
}