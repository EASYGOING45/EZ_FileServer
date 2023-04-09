/*
 *utils.h 用于存放一些常用的函数
 */
#ifndef UTILS_H
#define UTILS_H
#include <iostream>
#include <ctime>
#include <chrono>   //计时器
#include <string.h> //字符串处理

#include <sys/time.h>  //时间处理
#include <sys/epoll.h> //epoll 用于多路复用
#include <sys/fcntl.h> //fcntl 用于设置非阻塞

// 以 "09:50:19.0619 2022-09-26 [logType]: " 格式返回当前的时间和输出类型，logType 指定输出的类型：
// init :  表示服务器的初始化过程
// error ：表示服务器运行中的出错信息
// info ： 表示程序的运行信息
std::string outHead(const std::string logType);

// 向epollfd添加文件描述符，并指定监听事件。edgeTrigger:边缘触发 isOneshot：EPOLLONESHOT 事件
int addWaitFd(int epollFd, int newFd, bool edgeTrigger = false, bool isOneshot = false);

// 修改正在监听文件描述符的事件。edgeTrigger:是否为边沿触发，resetOneshot:是否重置EPOLLONESHOT事件 addEpollout:是否监听输出事件
int modifyWaitFd(int epollFd, int modFd, bool edgeTrigger = false, bool resetOneshot = false, bool addEpollout = false);

// 删除正在监听的文件描述符
int deleteWaitFd(int epollFd, int deleteFd);

// 设置文件描述符为非阻塞
int setNonBlocking(int fd);
#endif
