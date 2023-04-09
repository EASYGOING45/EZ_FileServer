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

#endif
