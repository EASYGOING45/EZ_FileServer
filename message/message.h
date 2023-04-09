/**
 * @file message/message.h
 * @brief Message class
 * 该文件中定义的类用于在事件处理过程中表示“请求消息”和“响应消息”
 * Message是基类，用来保存两种消息共有的信息；消息处理的状态和消息首部的信息
 * Request表示浏览器的请求消息，用来记录其中的重要字段，及事件处理中对请求消息处理了多少
 * Response表示向客户端回复的响应消息，记录响应消息中待发送的数据，如状态行、首部、消息体、响应消息已发送多少
 */

#ifndef MESSAGE_H
#define MESSAGE_H
#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <unordered_map>

// 枚举 表示Request或 Response中数据的处理状态
enum MSGSTATUS
{
    HANDLE_INIT,     // 正在接收/发送头部数据（请求行、请求头）
    HANDLE_HEAD,     // 正在接收/发送消息首部
    HANDLE_BODY,     // 正在接收/发送消息体
    HANDLE_COMPLATE, // 所有数据都已经处理完成
    HANDLE_ERROR,    // 处理过程中发生错误
};

// 表示消息体的类型
enum MSGBODYTYPE
{
    FILE_TYPE,  // 消息体是文件
    HTML_TYPE,  // 消息体是HTML页面
    EMPTY_TYPE, // 消息体为空
};

// 当接收文件时，消息体会分不同的部分，用该类型表示文件消息体已经处理到哪个部分
enum FILEMSGBODYSTATUS
{
    FILE_BEGIN_FLAG, // 正在获取并处理表示文件开始的标志行
    FILE_HEAD,       // 正在获取并处理文件属性部分
    FILE_CONTENT,    // 正在获取并处理文件内容的部分
    FILE_COMPLATE    // 文件已经处理完成
};

// 定义Request和Response公共的部分，即消息首部、消息体（可以获取消息首部的某个字段、修改与获取消息体相关的数据）
class Message
{
public:
    Message() : status(HANDLE_INIT)
    {
    }

public:
    // 请求消息和响应消息需要的成员体
    MSGSTATUS status; // 记录消息的接收状态，表示整个请求报文收到了多少以及发送了多少

    std::unordered_map<std::string, std::string> msgHeader; // 保存消息首部
private:
};

// 继承自Message，对请求行的修改和获取，保存收到的首部选项
class Request : public Message
{
public:
    Request() : Message()
    {
    }
    // 设置与返回请求行相关字段
    void setRequestLine(const std::string &requestLine)
    {
        std::istringstream lineStream(requestLine);

        // 获取请求方法
        lineStream >> requestMethod;

        // 获取请求资源
        lineStream >> requestResource;

        // 获取http版本
        lineStream >> httpVersion;
    }

    // 对于Request报文，根据传入的一行首部字符串，向首部保存选项
    void addHeaderOpt(const std::string &headLine)
    {
        static std::istringstream lineStream;
        lineStream.str(headLine); // 以 istringstream的方式处理头部选项

        std::string key, value; // 保存键值的临时量
        lineStream >> key;      // 获取key
        key.pop_back();         // 删除键中的冒号
        lineStream.get();       // 删除冒号后的空格

        // 读取空格之后所有的数据，遇到 \n 停止，所以 value 中还包含一个 \r
        getline(lineStream, value);
        value.pop_back(); // 删除其中的 \r

        if (key == "Content-Length")
        {
            // 保存消息体的长度
            contentLength = std::stoll(value);
        }
        else if (key == "Content-Type")
        {
            // 分离消息体类型。消息体类型可能是复杂的消息体，类似 Content-Type: multipart/form-data; boundary=---------------------------24436669372671144761803083960

            // 先找出值中分号的位置
            std::string::size_type semIndex = value.find(';');
            // 根据分号查找的结果，保存类型的结果
            if (semIndex != std::string::npos)
            {
                msgHeader[key] = value.substr(0, semIndex);
                std::string::size_type eqIndex = value.find('=', semIndex);
                key = value.substr(semIndex + 2, eqIndex - semIndex - 2);
                msgHeader[key] = value.substr(eqIndex + 1);
            }
            else
            {
                msgHeader[key] = value;
            }
        }
        else
        {
            msgHeader[key] = value;
        }
    }

public:
    // 数据成员字段
    std::string recvMsg; // 收到但是还未处理的数据

    std::string requestMethod;   // 请求消息的请求方法
    std::string requestResource; // 请求的资源
    std::string httpVersion;     // 请求的HTTP版本

    long long contentLength = 0; // 记录消息体的长度
    long long msgBodyRecvLen;    // 已经接收的消息体长度

    std::string recvFileName;        // 如果客户端发送的是文件，记录文件的名字
    FILEMSGBODYSTATUS fileMsgStatus; // 记录表示文件的消息体已经
};

#endif
