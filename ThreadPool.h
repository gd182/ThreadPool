#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <memory>
#include <exception>
#include <future>
#include <mutex>
#include <condition_variable>
#include "QueueMutex.h"

namespace tp
{
    namespace component
    {
        /**
         * @brief Structure representing a single thread in the pool
         * @brief ���������, �������������� ��������� ����� � ����
         */
        struct SingThread
        {
            std::unique_ptr<std::thread> thread;          // Thread object / ������ ������
            std::shared_ptr<std::atomic<bool>> isNotWorking; // Flag indicating if thread is working / ���� ������ ������
        };
    }

    /**
     * @brief Thread Pool class for managing and executing tasks concurrently
     * @brief ����� ���� ������� ��� ���������� � ���������� ����� �����������
     *
     * This class provides a flexible thread pool that can dynamically resize
     * and execute tasks asynchronously with future support.
     *
     * ���� ����� ������������� ������ ��� �������, ������� ����� �����������
     * �������� ������ � ��������� ������ ���������� � ���������� future.
     */
    class ThreadPool
    {
    public:
        // CONSTRUCTORS & DESTRUCTOR
        // ������������ � ����������

        ThreadPool();
        ThreadPool(int countThreads);
        ~ThreadPool();

        // POOL MANAGEMENT
        // ���������� �����

        void resize(int countThreads);
        void clearQueue();
        void stop(bool isWait = false);
        int size() { return threads.size(); };

        // TASK OPERATIONS
        // �������� � ��������

        std::function<void(int)> pop();

        /**
         * @brief Push a task with arguments to the queue
         * @brief ���������� ������ � ����������� � �������
         *
         * @tparam F Function type / ��� �������
         * @tparam Rest Argument types / ���� ����������
         * @return std::future for getting the result / std::future ��� ��������� ����������
         */
        template<typename F, typename... Rest>
        auto push(F&& f, Rest&&... rest) -> std::future<decltype(f(0, rest...))>
        {
            auto pck = std::make_shared<std::packaged_task<decltype(f(0, rest...))(int)>>(
                std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Rest>(rest)...)
            );
            auto fun = new std::function<void(int id)>([pck](int id) {
                (*pck)(id);
                });
            this->queue.push(fun);
            std::unique_lock<std::mutex> lock(this->mutex);
            this->cv.notify_one();
            return pck->get_future();
        };
        /**
         * @brief Push a simple task to the queue
         * @brief ���������� ������� ������ � �������
         *
         * @tparam F Function type / ��� �������
         * @return std::future for getting the result / std::future ��� ��������� ����������
         */
        template<typename F>
        auto push(F&& f) -> std::future<decltype(f(0))>
        {
            auto pck = std::make_shared<std::packaged_task<decltype(f(0))(int)>>(std::forward<F>(f));
            auto fun = new std::function<void(int id)>([pck](int id) {
                (*pck)(id);
                });
            this->queue.push(fun);
            std::unique_lock<std::mutex> lock(this->mutex);
            this->cv.notify_one();
            return pck->get_future();
        };

        // DELETED COPY AND MOVE SEMANTICS
        // ��������� ��������� ����������� � �����������
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        /**
         * @brief Get the number of idle threads in the pool
         * @brief �������� ���������� �������������� ������� � ����
         *
         * @return int Number of threads currently waiting for tasks
         * @return int ���������� �������, ��������� ������ � ������ ������
         */
        int numIdle() { return this->numWaiting; }

        /**
         * @brief Get reference to a specific thread by index
         * @brief �������� ������ �� ���������� ����� �� �������
         *
         * @param i Index of the thread (0 <= i < size())
         * @param i ������ ������ (0 <= i < size())
         * @return std::thread& Reference to the thread object
         * @return std::thread& ������ �� ������ ������
         *
         * @throw std::out_of_range if index is invalid
         * @throw std::out_of_range ���� ������ ��������������
         *
         * Warning: Use with caution! Direct thread manipulation can be dangerous.
         * ��������������: ����������� � �������������! ������ ���������� �������� ����� ���� �������.
         */
        std::thread& get_thread(int i) {
            if (i < 0 || i >= threads.size()) {
                throw std::out_of_range("Thread index out of range");
            }
            return *this->threads[i].thread;
        }

    private:
        // INTERNAL METHODS
        // ���������� ������

        void init();
        void setThread(int ind);

        // MEMBER VARIABLES
        // �����-������

        std::vector<tp::component::SingThread> threads;   // Collection of worker threads / ��������� ������� �������
        tp::component::QueueMutex<std::function<void(int id)>*> queue; // Task queue / ������� �����

        std::atomic<bool> isDone;     // Flag indicating completion / ���� ���������� ������
        std::atomic<bool> isStop;     // Flag indicating immediate stop / ���� ����������� ���������
        std::atomic<int> numWaiting;  // Number of waiting threads / ���������� ��������� �������

        std::mutex mutex;             // Mutex for synchronization / ������� ��� �������������
        std::condition_variable cv;   // Condition variable for task notification / �������� ���������� ��� �����������
    };
}

#endif // THREAD_POOL_H