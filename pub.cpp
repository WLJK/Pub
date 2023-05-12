/**********************************************************
*****************发布端程序publisher.cpp********************
***********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <condition_variable>

/* IDL_TypeSupport.h中包含所有依赖的头文件 */
#include "IDL_TypeSupport.h"

std::condition_variable reply_received_condition;
std::mutex reply_mutex;

auto start_time = std::chrono::high_resolution_clock::now(); // 记录第一个数据发送时间
auto end_time = std::chrono::high_resolution_clock::now();   // 记录最后一个数据发送时间
static char data_put = 1;
static long sent_packets = 0;
// 超时等待回复的超时时间（以毫秒为单位）
const int reply_timeout = 10000; // 10秒

std::atomic<int> throughput_counter(0);
std::mutex counter_mutex;

//使用OpenSSL库计算MD5哈希
std::string calculate_MD5(const std::string &input) {
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input.c_str(), input.size(), hash);

    std::ostringstream ss;
    for(int i = 0; i < MD5_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

    return ss.str();
}

class UserDataTypeListener : public DataReaderListener {
public:
    virtual void on_data_available(DataReader* reader);
};

/* 重写继承过来的方法on_data_available()，在其中进行数据监听读取操作 */
void UserDataTypeListener::on_data_available(DataReader* reader)
{
    UserDataTypeDataReader *UserDataType_reader = NULL;
    UserDataTypeSeq data_seq;
    SampleInfoSeq info_seq;
    ReturnCode_t retcode;
    int i;

    /* 利用reader，创建一个读取UserDataType类型的UserDataType_reader*/
    UserDataType_reader = UserDataTypeDataReader::narrow(reader);
    if (UserDataType_reader == NULL) {
        fprintf(stderr, "DataReader narrow error\n");
        return;
    }

    /* 获取数据，存放至data_seq，data_seq是一个队列 */
    retcode = UserDataType_reader->read(
            data_seq, info_seq, 10, 0, 0, 0);

    if (retcode == RETCODE_NO_DATA) {
        return;
    }
    else if (retcode != RETCODE_OK) {
        fprintf(stderr, "take error %d\n", retcode);
        return;
    }

    /* 打印数据 */
    /* 建议1：避免在此进行复杂数据处理 */
    /* 建议2：将数据传送到其他数据处理线程中进行处理 *
    /* 建议3：假如数据结构中有string类型，用完后需手动释放 */
    for (i = 0; i < data_seq.length(); ++i) {

        //UserDataTypeTypeSupport::print_data(&data_seq[i]);


        // 发送回复信号
        std::lock_guard<std::mutex> reply_lock(reply_mutex);
        reply_received_condition.notify_all();
    }

}


/* 删除所有实体 */
static int publisher_shutdown(DomainParticipant *participant)
{
	ReturnCode_t retcode; 
	int status = 0;

	if (participant != NULL) {
		retcode = participant->delete_contained_entities();
		if (retcode != RETCODE_OK) {
			fprintf(stderr, "delete_contained_entities error %d\n", retcode);
			status = -1;
		}

		retcode = DomainParticipantFactory::get_instance()->delete_participant(participant);
		if (retcode != RETCODE_OK) {
			fprintf(stderr, "delete_participant error %d\n", retcode);
			status = -1;
		}
	}

	return status;
}

/* 发布者函数 */
extern "C" int publisher_main(int domainId, int sample_count, int data_size)
{
	DomainParticipant *participant = NULL;
	Publisher *publisher = NULL;
    Topic* publish_topic = NULL;
    Topic* subscribe_topic = NULL;
    DataWriter *writer = NULL;
	UserDataTypeDataWriter * UserDataType_writer = NULL;
	UserDataType *instance = NULL;
	ReturnCode_t retcode;
	InstanceHandle_t instance_handle = HANDLE_NIL;
	const char *type_name = NULL;
	int count = 0;
    DataReader *reader = NULL;
    Subscriber *subscriber = NULL;


    UserDataTypeListener *reader_listener = NULL;
	/* 1. 创建一个participant，可以在此处定制participant的QoS */

	participant = DomainParticipantFactory::get_instance()->create_participant(
		domainId, PARTICIPANT_QOS_DEFAULT/* participant默认QoS */,
		NULL /* listener */, STATUS_MASK_NONE);
	if (participant == NULL) {
		fprintf(stderr, "create_participant error\n");
		publisher_shutdown(participant);
		return -1;
	}

    /* 2. 创建一个subscriber，可以在创建subscriber的同时定制其QoS  */

    subscriber = participant->create_subscriber(
            SUBSCRIBER_QOS_DEFAULT/* 默认QoS */,
            NULL /* listener */, STATUS_MASK_NONE);
    if (subscriber == NULL) {
        fprintf(stderr, "create_subscriber error\n");
        publisher_shutdown(participant);
        return -1;
    }

	/* 2. 创建一个publisher，可以在创建publisher的同时定制其QoS  */

	publisher = participant->create_publisher(
		PUBLISHER_QOS_DEFAULT /* 默认QoS */, 
		NULL /* listener */, STATUS_MASK_NONE);
	if (publisher == NULL) {
		fprintf(stderr, "create_publisher error\n");
		publisher_shutdown(participant);
		return -1;
	}

	/* 3. 在创建主题之前注册数据类型 */

	type_name = UserDataTypeTypeSupport::get_type_name();
	retcode = UserDataTypeTypeSupport::register_type(
		participant, type_name);
	if (retcode != RETCODE_OK) {
		fprintf(stderr, "register_type error %d\n", retcode);
		publisher_shutdown(participant);
		return -1;
	}

    /* 4. 创建主题，并定制主题的QoS  */


    publish_topic = participant->create_topic(
            "Topic_A" /* 发布主题名 */,
            type_name /* 类型名 */, TOPIC_QOS_DEFAULT /* 默认QoS */,
            NULL /* listener */, STATUS_MASK_NONE);
    if (publish_topic == NULL) {
        fprintf(stderr, "create_topic for publish error\n");
        publisher_shutdown(participant);
        return -1;
    }

	/* 4. 创建主题，并定制主题的QoS  */

    subscribe_topic = participant->create_topic(
            "topic_B" /* 订阅主题名 */,
            type_name /* 类型名 */, TOPIC_QOS_DEFAULT /* 默认QoS */,
            NULL /* listener */, STATUS_MASK_NONE);
    if (subscribe_topic == NULL) {
        fprintf(stderr, "create_topic for subscribe error\n");
        publisher_shutdown(participant);
        return -1;
    }
    //auto pool = new thread_pool<SendDataTask>(participant, topic, publisher);

    /* 5. 创建一个监听器 */
    reader_listener = new UserDataTypeListener();

    reader = subscriber->create_datareader(
            subscribe_topic, DATAREADER_QOS_DEFAULT,
            reader_listener, STATUS_MASK_ALL);
    if (reader == NULL) {
        fprintf(stderr, "create_datareader error\n");
        publisher_shutdown(participant);
        delete reader_listener;
        return -1;
    }

    /* 5. 创建datawriter，并定制datawriter的QoS  */

	writer = publisher->create_datawriter(
            publish_topic , DATAWRITER_QOS_DEFAULT/* 默认QoS */,
	NULL /* listener */, STATUS_MASK_NONE);
    if (writer == NULL) {
        fprintf(stderr, "create_datawriter error\n");
        publisher_shutdown(participant);
        return -1;
    }
    UserDataType_writer = UserDataTypeDataWriter::narrow(writer);
    if (UserDataType_writer == NULL) {
        fprintf(stderr, "DataWriter narrow error\n");
        publisher_shutdown(participant);
        return -1;
    }

	/* 6. 创建一个数据样本 */
	/* 建议：该数据为new出来的，使用后用户需要调用delete_data进行释放内存*/
	instance = UserDataTypeTypeSupport::create_data();
	if (instance == NULL) {
		fprintf(stderr, "UserDataTypeTypeSupport::create_data error\n");
		publisher_shutdown(participant);
		return -1;
	}

    // 发送数据并等待回复
    while (count < sample_count) {
        instance->a = new char[data_size];
        memset(instance->a, data_put, data_size);
        instance->MD5 = new char[33];

        std::string md5 = calculate_MD5(std::string(instance->a, data_size));
        strcpy(instance->MD5, md5.c_str());
        ++sent_packets;
        instance->sent_packets = sent_packets;

        // 发送数据
        retcode = UserDataType_writer->write(*instance, instance_handle);
        if (retcode != RETCODE_OK) {
            fprintf(stderr, "write error %d\n", retcode);
        } else {
            std::lock_guard<std::mutex> lock(counter_mutex);
            ++throughput_counter;

            if (count == 0) {   //第一次
                start_time = std::chrono::high_resolution_clock::now();
            }

            if (count == sample_count - 1) {    //最后一次
                end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                double average_latency = static_cast<double>(duration.count()) / sample_count / 2;
                std::cout << "平均端到端时延: " << average_latency << " 微秒" << std::endl;
            }
        }

        // 等待回复或超时
        std::unique_lock<std::mutex> reply_lock(reply_mutex);
        if (reply_received_condition.wait_for(reply_lock, std::chrono::milliseconds(reply_timeout)) == std::cv_status::timeout) {
            // 超时，执行重发操作
            fprintf(stderr, "超时重发\n");
            delete[] instance->a;
            delete[] instance->MD5;
            continue; // 继续下一次循环，重发数据
        }
        // 收到回复，增加计数器
        data_put++;
        ++count;
    }


    /* 8. 删除数据样本 */
	retcode = UserDataTypeTypeSupport::delete_data(instance);
	if (retcode != RETCODE_OK) {
		fprintf(stderr, "UserDataTypeTypeSupport::delete_data error %d\n", retcode);
	}

    //delete pool;
	/* 9. 删除所有实例 */
    delete reader_listener;
	return publisher_shutdown(participant);
}

//简单定时吞吐量
void print_throughput(int data_size) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        int throughput;
        {
            std::lock_guard<std::mutex> lock(counter_mutex);
            throughput = throughput_counter;
            throughput_counter = 0;
        }

        std::cout << "发布端吞吐量: " << data_size * 8 * throughput << " bit/s" << std::endl;
    }
}


int main(int argc, char *argv[])
{
    int domain_id = 0;
    int data_size = 1024;  //数据大小
    int sample_count = 0; /* 无限循环 */

    if (argc >= 2) {
        domain_id = atoi(argv[1]);  /* 发送至域domain_id */
        std::cout << "domain_id  :" <<domain_id <<std::endl;
    }
    if (argc >= 3) {
        sample_count = atoi(argv[2]); /* 发送sample_count次 */
        std::cout << "sample_count  :" <<sample_count<<std::endl;
    }
    if (argc >= 4) {
        data_size = atoi(argv[3]); /* 发送数据大小 */
        if (data_size < 1  || data_size > 1048576)
            return -1;
        std::cout << "data_size  :" <<data_size<<std::endl;
    }
    // 创建定时器线程
    std::thread timer_thread(print_throughput, data_size);
    timer_thread.detach();

    return publisher_main(domain_id, sample_count, data_size);
}
