#include "ThreadPool.h"
#include <iostream>

// CONSTRUCTORS & DESTRUCTOR
// ������������ � ����������

tp::ThreadPool::ThreadPool(TypePool typePool)
{
    init(typePool);

    // Use hardware concurrency as default thread count
    // ���������� ���������� �������������� ��� ���������� ������� �� ���������
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
        numThreads = 1; // Fallback if hardware_concurrency returns 0 / ��������� ������� ���� hardware_concurrency ���������� 0
    }

    threads.resize(numThreads);

    // Create and configure worker threads
    // �������� � ��������� ������� �������
    for (int i = 0; i < numThreads; ++i)
    {
        threads[i].isNotWorking = std::make_shared<std::atomic<bool>>(false);
        setThread(i);
    }
}

tp::ThreadPool::ThreadPool(unsigned int numThreads, TypePool typePool)
{
    // Initialize pool state
    // ������������� ��������� ����
    init(typePool);
    threads.resize(numThreads);

    // Create and configure worker threads
    // �������� � ��������� ������� �������
    for (int i = 0; i < numThreads; ++i)
    {
        threads[i].isNotWorking = std::make_shared<std::atomic<bool>>(false);
        setThread(i);
    }
}

tp::ThreadPool::~ThreadPool()
{
    // Graceful shutdown - wait for tasks to complete
    // ������� ���������� - �������� ���������� �����
    stop(true);
}

// POOL MANAGEMENT METHODS
// ������ ���������� �����

void tp::ThreadPool::stop(bool isWait)
{
    if (!isWait) {
        // Immediate stop - clear queue and force threads to stop
        // ����������� ��������� - ������� ������� � �������������� ��������� �������
        if (isStop)
            return;

        isStop = true;
        for (int i = 0, n = size(); i < n; ++i) {
            *threads[i].isNotWorking = true;
        }
        this->clearQueue();
    }
    else {
        // Graceful stop - allow tasks to complete naturally
        // ������� ��������� - ��������� ������� ����������� �����������
        if (isDone || isStop)
            return;
        isDone = true;
    }

    {
        // Notify all waiting threads to wake up and check conditions
        // ����������� ���� ��������� ������� ��� �������� �������
        std::unique_lock<std::mutex> lock(this->mutex);
        cv.notify_all();
    }

    // Wait for all threads to finish execution
    // �������� ���������� ���������� ���� �������
    for (int i = 0, n = size(); i < n; ++i) {
        if (threads[i].thread && threads[i].thread->joinable())
            threads[i].thread->join();
    }

    // Cleanup resources
    // ������� ��������
    clearQueue();
    threads.clear();
}

void tp::ThreadPool::resize(unsigned int numThreads)
{
    if (!isStop && !isDone) {
        int oldNumThread = threads.size();

        if (oldNumThread <= numThreads) {
            // Increase thread count - add new worker threads
            // ���������� ���������� ������� - ���������� ����� ������� �������
            threads.resize(numThreads);
            for (int i = oldNumThread; i < numThreads; ++i)
            {
                threads[i].isNotWorking = std::make_shared<std::atomic<bool>>(false);
                setThread(i);
            }
        }
        else {
            // Decrease thread count - stop excess threads
            // ���������� ���������� ������� - ��������� ������ �������
            for (int i = oldNumThread - 1; i >= numThreads; --i)
            {
                *threads[i].isNotWorking = true;
                threads[i].thread->detach();
            }

            // Notify detached threads to stop
            // ����������� ������������� ������� �� ���������
            std::unique_lock<std::mutex> lock(mutex);
            cv.notify_all();
            lock.unlock();

            threads.resize(numThreads);
        }
    }
}

void tp::ThreadPool::clearQueue()
{
    std::function<void(int id)>* fun;

    // Delete all pending tasks to prevent memory leaks
    // �������� ���� ��������� ����� ��� �������������� ������ ������
    while (queue->pop(fun))
        delete fun;
}

// INTERNAL METHODS
// ���������� ������

std::thread& tp::ThreadPool::getThread(int i)
{
    if (i < 0 || i >= threads.size()) {
        throw std::out_of_range("Thread index out of range");
    }
    return *this->threads[i].thread;
}

void tp::ThreadPool::init(TypePool typePool)
{
    this->typePool = typePool;

    if (typePool == TypePool::Normal)
    {
        queue = std::make_unique <tp::component::NormalQueue<std::function<void(int id)>*>>();
    }
    else
    {
        queue = std::make_unique <tp::component::PriorityQueue<std::function<void(int id)>*>>();
    }
    numWaiting = 0;     // No threads waiting initially
    isStop = false;     // Not stopped
    isDone = false;     // Not done
}

void tp::ThreadPool::setThread(int ind)
{
    std::shared_ptr<std::atomic<bool>> flag(threads[ind].isNotWorking);

    // Lambda function that represents the worker thread's lifecycle
    // ������-�������, �������������� ��������� ���� �������� ������
    auto f = [this, ind, flag]() {
        std::atomic<bool>& _flag = *flag;
        std::function<void(int id)>* fun;
        bool isPop = queue->pop(fun);

        // Main worker thread loop
        // �������� ���� �������� ������
        while (true) {
            // Process available tasks from the queue
            // ��������� ��������� ����� �� �������
            while (isPop) {
                // Smart pointer for automatic memory management
                // ����� ��������� ��� ��������������� ���������� �������
                std::unique_ptr<std::function<void(int id)>> func(fun);

                try {
                    // Execute the task with thread ID
                    // ���������� ������ � ��������������� ������
                    (*fun)(ind);
                }
                catch (const std::exception& e) {
                    // Handle standard exceptions gracefully
                    // ��������� ����������� ����������
                    std::cerr << "Exception in thread " << ind << ": " << e.what() << std::endl;
                }
                catch (...) {
                    // Handle unknown exceptions
                    // ��������� ����������� ����������
                    std::cerr << "Unknown exception in thread " << ind << std::endl;
                }

                if (_flag)
                    return;  // Exit if thread should stop
                else
                    isPop = queue->pop(fun);
            }

            // Wait for new tasks when queue is empty
            // �������� ����� ����� ��� ������ �������
            std::unique_lock<std::mutex> lock(mutex);
            ++numWaiting;

            // Wait for notification or condition change
            // �������� ����������� ��� ��������� �������
            cv.wait(lock, [this, &fun, &isPop, &_flag]() {
                isPop = queue->pop(fun);
                return isPop || isDone || _flag;
                });

            --numWaiting;

            if (!isPop)
                return;  // Exit if termination signaled
        }
        };

    // Create and start the worker thread
    // �������� � ������ �������� ������
    threads[ind].thread.reset(new std::thread(f));
}

// TASK OPERATIONS
// �������� � ��������

std::function<void(int)> tp::ThreadPool::pop()
{
    std::function<void(int id)>* fun = nullptr;
    queue->pop(fun);

    // Smart pointer for automatic cleanup
    // ����� ��������� ��� �������������� �������
    std::unique_ptr<std::function<void(int id)>> func(fun);
    std::function<void(int)> f;

    if (fun)
        f = *fun;

    return f;
}