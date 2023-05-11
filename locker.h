//信号量，条件变量
// Created by wljk
//

#ifndef MYTINYTCPSERVER_LOCKER_H
#define MYTINYTCPSERVER_LOCKER_H

#include <exception>
#include <mutex>
#include <condition_variable>

class sem
{
public:
    sem()
    : m_count(0)
    {
    }

    explicit sem(int num)
    : m_count(num)
    {
    }

    // 禁用复制构造函数和复制赋值运算符
    sem(const sem&) = delete;
    sem& operator=(const sem&) = delete;

    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_count == 0)
        {
            m_cv.wait(lock);
        }
        --m_count;
    }

    void post()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        ++m_count;
        m_cv.notify_one();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    int m_count;
};

class cond
{
public:
    cond() = default;

    void wait(std::unique_lock<std::mutex>& lock)
    {
        m_cond.wait(lock);
    }

    bool timewait(std::unique_lock<std::mutex>& lock, const std::chrono::system_clock::time_point& tp)
    {
        return m_cond.wait_until(lock, tp) == std::cv_status::no_timeout;
    }

    void signal()
    {
        m_cond.notify_one();
    }

    void broadcast()
    {
        m_cond.notify_all();
    }

private:
    std::condition_variable m_cond;
};


#endif //MYTINYTCPSERVER_LOCKER_H
