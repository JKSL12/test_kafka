#include "rdkafkacpp.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

// Callback
class ExampleDeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb(RdKafka::Message& message) override {
        std::cerr << "[CALLBACK] ";
        if (message.err())
            std::cerr << "% Message delivery failed: " << message.errstr() << std::endl;
        else
            std::cerr << "% Message delivered to topic " << message.topic_name()
            << " [" << message.partition() << "] at offset " << message.offset() << std::endl;
    }
};

int main()
{
    std::string errmsg;
    ExampleDeliveryReportCb ex_dr_cb;
    RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    conf->set("bootstrap.servers", "localhost:9092,localhost:9093", errmsg);
    conf->set("dr_cb", &ex_dr_cb, errmsg);

    RdKafka::Producer* producer = RdKafka::Producer::create(conf, errmsg);
    if (!producer) {
        std::cerr << "Failed to create producer: " << errmsg << std::endl;
        return 1;
    }

    std::string topic = "newtopic";
    std::string message = "abcdefg";

    delete conf;

    while (true) {
        RdKafka::ErrorCode err = producer->produce(
            topic,
            RdKafka::Topic::PARTITION_UA,
            RdKafka::Producer::RK_MSG_COPY,
            const_cast<char*>(message.data()),
            message.length(),
            nullptr, 0, 0, nullptr, nullptr
        );

        std::cerr << "[MAIN] ";
        if (err != RdKafka::ERR_NO_ERROR) {
            std::cerr << "Failed to produce to topic " << topic << ": " << RdKafka::err2str(err) << std::endl;
            if (err == RdKafka::ERR__QUEUE_FULL) {
                producer->poll(1000); // Block for max 1000ms
            }
        }
        else {
            std::cerr << "Enqueued message (" << message.size() << " bytes) for topic " << topic << std::endl;
        }

        producer->poll(1000); // Ensure delivery reports are processed

        producer->flush(10 * 1000); // Wait for max 10 seconds
        if (producer->outq_len() > 0) {
            std::cerr << "% " << producer->outq_len() << " message(s) were not delivered" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    delete producer;
    return 0;
}