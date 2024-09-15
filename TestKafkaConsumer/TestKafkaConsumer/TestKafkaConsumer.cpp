#include "rdkafkacpp.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

// Callback for logging events
class ExampleEventCb : public RdKafka::EventCb {
public:
    void event_cb(RdKafka::Event& event) override {
        switch (event.type()) {
        case RdKafka::Event::EVENT_ERROR:
            std::cerr << "ERROR: " << event.str() << std::endl;
            break;
        case RdKafka::Event::EVENT_STATS:
            std::cerr << "STATS: " << event.str() << std::endl;
            break;
        case RdKafka::Event::EVENT_LOG:
            std::cerr << "LOG: " << event.str() << std::endl;
            break;
        default:
            std::cerr << "OTHER: " << event.str() << std::endl;
            break;
        }
    }
};

// Callback for handling partition assignments and revocations
class ExampleRebalanceCb : public RdKafka::RebalanceCb {
public:
    void rebalance_cb(RdKafka::KafkaConsumer* consumer, RdKafka::ErrorCode err, std::vector<RdKafka::TopicPartition*>& partitions) override {
        if (err == RdKafka::ERR__ASSIGN_PARTITIONS) {
            std::cerr << "Assigning partitions" << std::endl;
            consumer->assign(partitions);
        }
        else if (err == RdKafka::ERR__REVOKE_PARTITIONS) {
            std::cerr << "Revoking partitions" << std::endl;
            consumer->unassign();
        }
        else {
            std::cerr << "Rebalance error: " << RdKafka::err2str(err) << std::endl;
        }
    }
};

int main() {
    std::string errstr;

    // Create configuration
    RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    if (!conf) {
        std::cerr << "Failed to create configuration" << std::endl;
        return 1;
    }

    // Set configuration options
    conf->set("metadata.broker.list", "localhost:9092,localhost:9093", errstr);
    conf->set("group.id", "example_group", errstr);
    conf->set("enable.auto.commit", "true", errstr);
    conf->set("auto.offset.reset", "earliest", errstr);

    ExampleEventCb ex_event_cb;
    conf->set("event_cb", &ex_event_cb, errstr);

    ExampleRebalanceCb ex_rebalance_cb;
    conf->set("rebalance_cb", &ex_rebalance_cb, errstr);

    // Create consumer
    RdKafka::KafkaConsumer* consumer = RdKafka::KafkaConsumer::create(conf, errstr);
    delete conf;

    if (!consumer) {
        std::cerr << "Failed to create consumer: " << errstr << std::endl;
        return 1;
    }

    // Subscribe to the topic
    std::string topic = "newtopic";
    RdKafka::ErrorCode err = consumer->subscribe({ topic });
    if (err != RdKafka::ERR_NO_ERROR) {
        std::cerr << "Failed to subscribe to topic " << topic << ": " << RdKafka::err2str(err) << std::endl;
        return 1;
    }

    // Consume messages
    while (true) {
        RdKafka::Message* msg = consumer->consume(1000);  // Consume with a timeout of 1000ms
        if (msg->err()) {
            if (msg->err() == RdKafka::ERR__PARTITION_EOF) {
                std::cerr << "%% End of partition event: " << msg->partition() << std::endl;
            }
            else {
                std::cerr << "Error: " << msg->errstr() << std::endl;
            }
        }
        else {
            std::cerr << "Received message: " << static_cast<const char*>(msg->payload()) << " " << msg->topic_name()
                << " [" << msg->partition() << "] at offset " << msg->offset() << std::endl;
        }
        delete msg;

        std::this_thread::sleep_for(std::chrono::seconds(1));  // Sleep to prevent high CPU usage
    }

    // Clean up
    consumer->unsubscribe();
    delete consumer;
    return 0;
}