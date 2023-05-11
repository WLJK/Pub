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

/* IDL_TypeSupport.h中包含所有依赖的头文件 */
#include "IDL_TypeSupport.h"

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
	Topic *topic = NULL;
	DataWriter *writer = NULL;
	UserDataTypeDataWriter * UserDataType_writer = NULL;
	UserDataType *instance = NULL;
	ReturnCode_t retcode;
	InstanceHandle_t instance_handle = HANDLE_NIL;
	const char *type_name = NULL;
	int count = 0;

	/* 1. 创建一个participant，可以在此处定制participant的QoS */
	/* 建议1：在程序启动后优先创建participant，进行资源初始化*/
	/* 建议2：相同的domainId只创建一次participant，重复创建会占用大量资源 */ 
	participant = DomainParticipantFactory::get_instance()->create_participant(
		domainId, PARTICIPANT_QOS_DEFAULT/* participant默认QoS */,
		NULL /* listener */, STATUS_MASK_NONE);
	if (participant == NULL) {
		fprintf(stderr, "create_participant error\n");
		publisher_shutdown(participant);
		return -1;
	}

	/* 2. 创建一个publisher，可以在创建publisher的同时定制其QoS  */
	/* 建议1：在程序启动后优先创建publisher */
	/* 建议2：一个participant下创建一个publisher即可，无需重复创建 */
	publisher = participant->create_publisher(
		PUBLISHER_QOS_DEFAULT /* 默认QoS */, 
		NULL /* listener */, STATUS_MASK_NONE);
	if (publisher == NULL) {
		fprintf(stderr, "create_publisher error\n");
		publisher_shutdown(participant);
		return -1;
	}

	/* 3. 在创建主题之前注册数据类型 */
	/* 建议1：在程序启动后优先进行注册 */
	/* 建议2：一个数据类型注册一次即可 */
	type_name = UserDataTypeTypeSupport::get_type_name();
	retcode = UserDataTypeTypeSupport::register_type(
		participant, type_name);
	if (retcode != RETCODE_OK) {
		fprintf(stderr, "register_type error %d\n", retcode);
		publisher_shutdown(participant);
		return -1;
	}

	/* 4. 创建主题，并定制主题的QoS  */
	/* 建议1：在程序启动后优先创建Topic */
	/* 建议2：一种主题名创建一次即可，无需重复创建 */
	topic = participant->create_topic(
		"Topic_A"/* 主题名 */,
		type_name /* 类型名 */, TOPIC_QOS_DEFAULT/* 默认QoS */,
		NULL /* listener */, STATUS_MASK_NONE);
	if (topic == NULL) {
		fprintf(stderr, "create_topic error\n");
		publisher_shutdown(participant);
		return -1;
	}
    //auto pool = new thread_pool<SendDataTask>(participant, topic, publisher);

	/* 5. 创建datawriter，并定制datawriter的QoS  */
	/* 建议1：在程序启动后优先创建datawriter */
	/* 建议2：创建一次即可，无需重复创建 */
	/* 建议3：在程序退出时再进行释放 */
	/* 建议4：避免打算发送数据时创建datawriter，发送数据后删除，该做法消耗资源，影响性能 */
	writer = publisher->create_datawriter(
	topic , DATAWRITER_QOS_DEFAULT/* 默认QoS */,
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
    char data_put = 1;
	/* 7. 主循环 ，发送数据 */
    long sent_packets = 0;

    for (count = 0; (sample_count == 0) || (count < sample_count); ++count) {

        instance->a = new char[data_size];
        memset(instance->a, data_put, data_size);
        instance->MD5 = new char[33];

        std::string md5 = calculate_MD5(std::string(instance->a, data_size));

        strcpy(instance->MD5, md5.c_str());
        //std::cout <<"strcpy: " << md5<<std::endl;
        ++sent_packets;
        instance->sent_packets = sent_packets; // 将计数器值存储到数据包

        retcode = UserDataType_writer->write(*instance, instance_handle);
        if (retcode != RETCODE_OK) {
            fprintf(stderr, "write error %d\n", retcode);
        }
        else {
            std::lock_guard<std::mutex> lock(counter_mutex);
            //delete[] m_instance->MD5;
            //delete[] m_instance->a;
            ++throughput_counter;

        }
        //pool->append(new SendDataTask( instance, instance_handle));

        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 每次迭代延迟10毫秒
        //sleep(1);//沉睡1秒
        data_put++;
    }

    //pool->wait_for_all_tasks();

    /* 8. 删除数据样本 */
	retcode = UserDataTypeTypeSupport::delete_data(instance);
	if (retcode != RETCODE_OK) {
		fprintf(stderr, "UserDataTypeTypeSupport::delete_data error %d\n", retcode);
	}

    //delete pool;
	/* 9. 删除所有实例 */
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
