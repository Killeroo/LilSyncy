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
        : q()
        , m()
        , c()
    {}

    ~SafeQueue(void)
    {}

    // Add an element to the queue.
    void enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
        c.notify_one();
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue(void)
    {
        std::unique_lock<std::mutex> lock(m);
        while (q.empty())
        {
            // release lock as long as the wait and reaquire it afterwards.
            c.wait(lock);
        }
        T val = q.front();
        q.pop();
        return val;
    }

    bool empty()
    {
        // Is this needed???????, I don't think we need this lock 
        std::lock_guard<std::mutex> lock(m);
        return q.empty();
    }

    int size()
    {
        std::lock_guard<std::mutex> lock(m);
        return q.size();
    }

    std::queue<T> getQueue()
    {
        return q;
    }

    void clear()
    {
        std::queue<T> empty;
        std::swap(q, empty);
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};