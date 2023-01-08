// MultipleProducer_MultipleConsumer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <array>
#include <cassert>
#include <optional>
#include <vector>
#include <functional>

struct SharedRingBuffer
{
    static constexpr size_t m_Capacity{ 10 };
    std::array<uint16_t, m_Capacity> m_Buffer{};
    std::size_t m_WriteIndex{};
    std::size_t m_ReadIndex{};

    std::mutex m_Lock;
    std::condition_variable m_Fulll;
    std::condition_variable m_Empty;

    bool IsFull()
    {
        return (m_WriteIndex + 1) % m_Capacity == m_ReadIndex;
    }

    bool IsEmpty()
    {
        return m_WriteIndex == m_ReadIndex;
    }
};

void Produce(SharedRingBuffer& Buffer)
{
    srand(time(0U));
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(Buffer.m_Lock);
            Buffer.m_Empty.wait(lock, [&] {
                return !Buffer.IsFull();
                });
            auto number = rand();
            Buffer.m_Buffer[Buffer.m_WriteIndex] = number;
            std::clog << "thread_id: " << std::this_thread::get_id() << ", produced: " << number << ", at " << Buffer.m_WriteIndex << std::endl;
            Buffer.m_WriteIndex = (Buffer.m_WriteIndex + 1) % Buffer.m_Capacity;
            Buffer.m_Fulll.notify_all();
        }
    }
}

void Consume(SharedRingBuffer& Buffer)
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(Buffer.m_Lock);
            Buffer.m_Fulll.wait(lock, [&] {
                return !Buffer.IsEmpty();
                });

            uint16_t data = Buffer.m_Buffer.at(Buffer.m_ReadIndex);
            std::clog << "\t\t\t\t\t"<< "thread_id: "<<std::this_thread::get_id() << ", consumed: " << data << ", from: " << Buffer.m_ReadIndex << std::endl;
            Buffer.m_ReadIndex = (Buffer.m_ReadIndex + 1) % Buffer.m_Capacity;
            Buffer.m_Empty.notify_all();
        }
    }
}

int main()
{
    SharedRingBuffer buffer;
    static constexpr uint16_t numberOfProducers{ 10 };
    static constexpr uint16_t numberOfConsumers{ 10 };

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (size_t i = 0; i < numberOfProducers; ++i)
    {
        producers.emplace_back(Produce, std::ref(buffer));
    }
    for (size_t i = 0; i < numberOfConsumers; ++i)
    {
        consumers.emplace_back(Consume, std::ref(buffer));
    }

    for (size_t i = 0; i < numberOfProducers; ++i)
    {
        producers[i].join();
    }
    for (size_t i = 0; i < numberOfConsumers; ++i)
    {
        consumers[i].join();
    }
    return 0;
}