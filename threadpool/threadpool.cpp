#include "threadpool.h"

ThreadPool::ThreadPool(int threadNum) : m_threadNum(threadNum)
{
    // 初始化互斥量
    int ret = pthread_mutex_init(&queueLocker, nullptr); // 互斥访问事件队列的互斥量 queueLocker
    if (ret != 0)
    {
        throw std::runtime_error("初始化互斥量失败");
    }

    // 初始化信号量
    ret = sem_init(&queueEventNum, 0, 0);
    if (ret != 0)
    {
        throw std::runtime_error("初始化信号量失败");
    }

    // 初始化线程池中的所有线程
    m_threads = new pthread_t[m_threadNum];
    for (int i = 0; i < m_threadNum; ++i)
    {
        ret = pthread_create(m_threads + i, nullptr, worker, this);
        if (ret != 0)
        {
            delete[] m_threads;
            throw std::runtime_error("线程创建失败");
        }
        ret = pthread_detach(m_threads[i]); // 线程分离 detach 之后，线程结束后会自动释放资源
        ++tnum;                             // 记录线程个数
        usleep(1000);                       // 为了在线程中记录线程序号
        if (ret != 0)
        {
            delete[] m_threads;
            throw std::runtime_error("设置脱离线程失败");
        }
    }
}

ThreadPool::~ThreadPool()
{
    // 释放互斥量
    pthread_mutex_destroy(&queueLocker);

    // 释放信号量
    sem_destroy(&queueEventNum);

    // 释放动态创建的保存线程id的数组
    delete[] m_threads;
}