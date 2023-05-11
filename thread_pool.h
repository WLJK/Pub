//
// Created by wljk
//

#ifndef MYTINYTCPSERVER_THREAD_POOL_H
#define MYTINYTCPSERVER_THREAD_POOL_H

#include <thread>
#include <exception>
#include <vector>
#include <list>
#include <iostream>
#include <atomic>
#include "locker.h"
#include "IDL_TypeSupport.h"


template <typename T>
class thread_pool
{
public:
    thread_pool(DomainParticipant* participant, Topic* topic, Publisher* publisher,int thread_number = 8, int max_request = 10000);
    ~thread_pool();
    bool append(T* request);
    void wait_for_all_tasks();
private:
    void run();

private:
    int m_thread_number;    //线程池中的数量
    int m_max_requests; //允许最大链接数
    std::vector<std::thread> m_threads;   //线程池数组
    std::list<T* > m_work_queue;    //请求队列
    std::mutex m_queue_locker;  //请求队列互斥锁
    sem m_queue_stat;    //是否有任务请求
    std::atomic<bool> m_stop;   //是否停止线程
    std::condition_variable m_all_tasks_done;
    int m_pending_tasks = 0;

    DomainParticipant* m_participant;
    Topic* m_topic;
    Publisher* m_publisher;
};

template <typename T>
thread_pool<T>::thread_pool(DomainParticipant* participant, Topic* topic, Publisher* publisher,int thread_number, int max_request)
        : m_participant(participant), m_topic(topic), m_publisher(publisher), m_thread_number(thread_number), m_max_requests(max_request), m_threads(thread_number)
{
            std::cout << "线程池启动"<<std::endl;
    if (thread_number <= 0 || max_request <= 0)
        throw std::exception();

    for(int i = 0; i < thread_number; ++i)
    {
        m_threads[i] = std::thread([this] {
            //std::cout << "m_threads run"<<std::endl;
            this->run();
        });
        //m_threads[i].detach();
    }
}

template <typename T>
thread_pool<T>::~thread_pool()
{
    m_stop = true;
    m_queue_stat.post(); // 唤醒线程停止
    for(auto& thread : m_threads)
    {
        if(thread.joinable())
        {
            thread.join();
        }
    }
}

template <typename T>
bool thread_pool<T>::append (T* request) {
    std::lock_guard<std::mutex> locker(m_queue_locker);
    if (m_work_queue.size() >= m_max_requests)
        return false;
    //std::cout << "append" <<std::endl;
    m_work_queue.push_back(request);
    ++m_pending_tasks;
    m_queue_stat.post();
    return true;
}

template <typename T>
void thread_pool<T>::run()
{
    // 创建数据写入者
    DataWriter* writer = m_publisher->create_datawriter(
            m_topic, DATAWRITER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (!writer)
    {
        std::cout << "创建数据写入者失败" <<std::endl;
        //tip!!!
        return;
    }

    UserDataTypeDataWriter *  MyData_writer = UserDataTypeDataWriter::narrow(writer);

    while(!m_stop)
    {
        //信号量等待
        m_queue_stat.wait();

        m_queue_locker.lock();
        if (m_work_queue.empty())
        {
            m_queue_locker.unlock();
            continue;
        }
        T *request = m_work_queue.front();
        m_work_queue.pop_front();
        m_queue_locker.unlock();

        if (!request)
            continue;

        request->execute(MyData_writer);

        std::lock_guard<std::mutex> locker(m_queue_locker);
        --m_pending_tasks;
    }
}

template <typename T>
void thread_pool<T>::wait_for_all_tasks() {
    std::unique_lock<std::mutex> lock(m_queue_locker);
    m_all_tasks_done.wait(lock, [this]{ return m_pending_tasks == 0; });
}

#endif //MYTINYTCPSERVER_THREAD_POOL_H
