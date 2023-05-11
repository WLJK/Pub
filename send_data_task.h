//
// Created by wljk on 23-5-11.
//

#ifndef PUB_SEND_DATA_TASK_H
#define PUB_SEND_DATA_TASK_H
#include <utility>

#include "IDL_TypeSupport.h"

class SendDataTask {
private:
    UserDataType* m_instance;
    InstanceHandle_t m_instance_handle;
public:
    static std::atomic<int> throughput_counter;
    static std::mutex counter_mutex;
    SendDataTask(UserDataType* instance, InstanceHandle_t instance_handle)
            : m_instance(instance), m_instance_handle(instance_handle)
            {}

    void execute(UserDataTypeDataWriter *  MyData_writer) {
        // 写入数据
        ReturnCode_t retcode = MyData_writer->write(*m_instance, m_instance_handle);
        if (retcode != RETCODE_OK) {
            fprintf(stderr, "write error %d\n", retcode);
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(counter_mutex);
                ++throughput_counter;
            }

            //fprintf(stderr, "write  successfully . . \n");
        }
    }
};

// 初始化静态变量
std::atomic<int> SendDataTask::throughput_counter(0);
std::mutex SendDataTask::counter_mutex;

#endif //PUB_SEND_DATA_TASK_H
