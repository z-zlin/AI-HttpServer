#include"../include/AIUtil/MQManager.h"
#include <iostream>

// ------------------- MQManager -------------------
MQManager::MQManager(size_t poolSize)
    : poolSize_(poolSize), counter_(0) {
    for (size_t i = 0; i < poolSize_; ++i) {
        auto conn = std::make_shared<MQConn>();
        try {
            //  Create
            conn->channel = AmqpClient::Channel::Create("localhost", 5672, "guest", "guest", "/");
            std::cerr << "[MQManager] channel created ok for pool index " << i
                      << " host=localhost port=5672 user=guest vhost=/"
                      << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[MQManager][ERROR] failed to create channel for index " << i
                      << " host=localhost port=5672 user=guest vhost=/"
                      << " reason=" << e.what() << std::endl;
            throw;
        }

        pool_.push_back(conn);
    }
}

void MQManager::publish(const std::string& queue, const std::string& msg) {
    size_t index = counter_.fetch_add(1) % poolSize_;
    auto& conn = pool_[index];

    std::lock_guard<std::mutex> lock(conn->mtx);
    try {
        auto message = AmqpClient::BasicMessage::Create(msg);
        conn->channel->BasicPublish("", queue, message);
    } catch (const std::exception& e) {
        std::cerr << "[MQManager][ERROR] BasicPublish failed on queue=" << queue
                  << " msgLen=" << msg.size()
                  << " reason=" << e.what() << std::endl;
        throw;
    }
}

// ------------------- RabbitMQThreadPool -------------------

void RabbitMQThreadPool::start() {
    for (int i = 0; i < thread_num_; ++i) {
        workers_.emplace_back(&RabbitMQThreadPool::worker, this, i);
    }
}

void RabbitMQThreadPool::shutdown() {
    stop_ = true;
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void RabbitMQThreadPool::worker(int id) {
    try {
        // Each thread has its own independent channel
        auto channel = AmqpClient::Channel::Create(rabbitmq_host_, 5672, "guest", "guest", "/");
        std::cerr << "[RabbitMQThreadPool] worker " << id
                  << " channel created ok host=" << rabbitmq_host_
                  << " port=5672 user=guest vhost=/"
                  << std::endl;
        // set exclusive
        channel->DeclareQueue(queue_name_, false, true, false, false);
        // Prevent channel error: 403: AMQP_BASIC_CONSUME_METHOD caused: ACCESS_REFUSED - queue 
        // 'sql_queue' in vhost '/' in exclusive use
        // std::string consumer_tag = channel->BasicConsume(queue_name_, "");
        std::string consumer_tag = channel->BasicConsume(queue_name_, "", true, false, false);

        channel->BasicQos(consumer_tag, 1); 

        while (!stop_) {
            AmqpClient::Envelope::ptr_t env;
            bool ok = channel->BasicConsumeMessage(consumer_tag, env, 500); // 500ms 
            if (ok && env) {
                std::string msg = env->Message()->Body();
                handler_(msg);          
                channel->BasicAck(env); 
            }
        }

        channel->BasicCancel(consumer_tag);
    }
    catch (const std::exception& e) {
        std::cerr << "Thread " << id << " exception: " << e.what()
                  << " (RabbitMQThreadPool worker, host=" << rabbitmq_host_
                  << " port=5672 user=guest vhost=/)"
                  << std::endl;
    }
}
