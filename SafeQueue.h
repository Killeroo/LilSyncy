#pragma once

#include <queue>
#include <mutex>

//////////////////////////////////////////////////
// A threadsafe-queue.
// Copied from: https://stackoverflow.com/a/16075550
//////////////////////////////////////////////////
template <class T>
class SafeQueue
{
public:
    SafeQueue(void)
        : Queue()
        , Mutex()
        , Conditional()
    {}

    ~SafeQueue(void)
    {}

    // Add an element to the queue.
    void Enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(Mutex);
        Queue.push(t);
        Conditional.notify_one();
    }

    // Get the "front"-element.
    // If the queue is Empty, wait till a element is avaiable.
    T Dequeue(void)
    {
        std::unique_lock<std::mutex> lock(Mutex);
        while (Queue.empty())
        {
            // release lock as long as the wait and reaquire it afterwards.
            Conditional.wait(lock);
        }
        T val = Queue.front();
        Queue.pop();
        return val;
    }

    bool Empty()
    {
        std::lock_guard<std::mutex> lock(Mutex);
        return Queue.empty();
    }

    size_t Size()
    {
        std::lock_guard<std::mutex> lock(Mutex);
        return Queue.size();
    }

    void Clear()
    {
        std::queue<T> empty;
        std::swap(Queue, empty);
    }

    std::queue<T> GetQueue()
    {
        return Queue;
    }

private:
    std::queue<T> Queue;
    mutable std::mutex Mutex;
    std::condition_variable Conditional;
};