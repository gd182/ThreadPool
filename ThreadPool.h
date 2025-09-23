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
        /**
         * @brief Type of thread pool queue
         * @brief ��� ������� ���� �������
         */
        enum class TypePool
        {
            Normal,   // Normal FIFO queue / ������� ������� FIFO
            Priority  // Priority-based queue / ������� �� ������ �����������
        };

        // CONSTRUCTORS & DESTRUCTOR
        // ������������ � ����������

        /**
         * @brief Constructor with default number of threads
         * @brief ����������� � ����������� ������� �� ���������
         *
         * @param typePool Type of queue to use / ��� ������������ �������
         */
        ThreadPool(TypePool typePool = TypePool::Normal);
        
        /**
         * @brief Constructor with specified number of threads
         * @brief ����������� � ��������� ����������� �������
         *
         * @param numThreads Number of worker threads / ���������� ������� �������
         * @param typePool Type of queue to use / ��� ������������ �������
         */
        ThreadPool(unsigned int countThreads, TypePool typePool = TypePool::Normal);
        
        /**
         * @brief Destructor - automatically stops the pool
         * @brief ���������� - ������������� ������������� ���
         */
        ~ThreadPool();

        // POOL MANAGEMENT
// ���������� �����

/**
 * @brief Resize the thread pool to the specified number of threads
 * @brief �������� ������ ���� ������� �� ���������� ���������� �������
 *
 * @param countThreads New number of threads in the pool / ����� ���������� ������� � ����
 *
 * @note If increasing size, new threads will be created immediately
 * @note ��� ���������� ������� ����� ������ ����� ������� ����������
 *
 * @note If decreasing size, excess threads will be stopped after completing current tasks
 * @note ��� ���������� ������� ������ ������ ����� ����������� ����� ���������� ������� �����
 *
 * @warning Does nothing if pool is stopped or stopping
 * @warning ������ �� ������, ���� ��� ���������� ��� ���������������
 */
        void resize(unsigned int countThreads);

        /**
         * @brief Clear all pending tasks from the queue
         * @brief ������� ��� ��������� ������ �� �������
         *
         * @note This will delete all tasks that have not yet been executed
         * @note ��� ������ ��� ������, ������� ��� �� ���� ���������
         *
         * @warning Tasks being executed by threads are not affected
         * @warning ������, ����������� �������� � ������ ������, �� �������������
         *
         * @warning Memory of task objects will be freed
         * @warning ������ �������� ����� ����� �����������
         */
        void clearQueue();

        /**
         * @brief Stop the thread pool
         * @brief ������������� ��� �������
         *
         * @param isWait If true, wait for current tasks to complete (graceful shutdown)
         *               If false, stop immediately (force shutdown)
         * @param isWait ���� true, ���� ���������� ������� ����� (������� ���������)
         *               ���� false, ������������� ���������� (�������������� ���������)
         *
         * @note After stopping, the pool cannot be restarted
         * @note ����� ��������� ��� ������ �������������
         *
         * @warning With isWait=false, queued tasks will be discarded without execution
         * @warning ��� isWait=false ������ � ������� ����� ��������� ��� ����������
         */
        void stop(bool isWait = false);

        /**
         * @brief Get the current number of threads in the pool
         * @brief �������� ������� ���������� ������� � ����
         *
         * @return int Number of worker threads / ���������� ������� �������
         *
         * @note This includes both active and idle threads
         * @note �������� ��� ��������, ��� � �������������� ������
         */
        int size() { return threads.size(); };

        // TASK OPERATIONS
        // �������� � ��������

        /**
         * @brief Pop a task from the front of the queue
         * @brief ��������� ������ �� ������ �������
         *
         * @return std::function<void(int)> The popped task, or empty function if queue was empty
         * @return std::function<void(int)> ����������� ������ ��� ������ �������, ���� ������� �����
         *
         * @note The task is removed from the queue
         * @note ������ ��������� �� �������
         *
         * @warning This operation is thread-safe but should be used with caution
         * @warning ��� �������� ���������������, �� ������ �������������� � �������������
         *
         * @warning If no task is available, returns an empty std::function
         * @warning ���� ����� ���, ���������� ������ std::function
         *
         * @example
         * auto task = pool.pop();
         * if (task) {
         *     task(123); // Execute the task with thread ID 123
         * }
         */
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
            return this->push(0, std::forward<F>(f), std::forward<Rest>(rest)...);
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
            return this->push(0, std::forward<F>(f));
        };

        /**
         * @brief Push a task with priority and arguments to the queue
         * @brief ���������� ������ � ����������� � ����������� � �������
         *
         * @tparam F Function type / ��� �������
         * @tparam Rest Argument types / ���� ����������
         * @param priority Task priority (higher = more important) / ��������� ������ (���� = ������)
         * @return std::future for getting the result / std::future ��� ��������� ����������
         */
        template<typename F, typename... Rest>
        auto push(int priority, F&& f, Rest&&... rest) -> std::future<decltype(f(0, rest...))>
        {
            auto pck = std::make_shared<std::packaged_task<decltype(f(0, rest...))(int)>>(
                std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Rest>(rest)...)
            );

            auto fun = new std::function<void(int id)>([pck](int id) {
                (*pck)(id);
                });

            // Use priority queue if available, otherwise normal push
            // ���������� ������� � ����������� ���� ��������, ����� ������� ����������
            if (typePool == TypePool::Priority) {
                auto* priorityQueue = dynamic_cast<component::PriorityQueue<std::function<void(int id)>*>*>(queue.get());
                if (priorityQueue) {
                    priorityQueue->push(fun, priority);
                }
                else {
                    queue->push(fun);
                }
            }
            else {
                queue->push(fun);
            }

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
        std::thread& getThread(int i);

        /**
         * @brief Get the type of queue used by the pool
         * @brief �������� ��� �������, ������������ �����
         *
         * @return TypePool Type of the queue / ��� �������
         */
        TypePool getQueueType() const { return typePool; }

        /**
         * @brief Check if the thread pool is currently running and accepting tasks
         * @brief ���������, �������� �� ��� ������� � ������ ������ � ��������� �� ������
         *
         * @return true if the pool is running and can accept new tasks
         * @return true ���� ��� �������� � ����� ��������� ����� ������
         *
         * @return false if the pool is stopped or in the process of stopping
         * @return false ���� ��� ���������� ��� ��������� � �������� ���������
         */
        bool isRunning() const { return !isDone && !isStop; }

        /**
         * @brief Check if the thread pool has been stopped or is stopping
         * @brief ���������, ��� �� ��� ������� ���������� ��� ��������� � �������� ���������
         *
         * @return true if the pool is stopped or in the process of stopping
         * @return true ���� ��� ���������� ��� ��������� � �������� ���������
         *
         * @return false if the pool is running and accepting tasks
         * @return false ���� ��� �������� � ��������� ������
         *
         * @note This is the inverse of isRunning()
         * @note ��� �������� ������� � isRunning()
         */
        bool isStopped() const { return isDone || isStop; }
    private:
        // INTERNAL METHODS
        // ���������� ������

        void init(TypePool typePool);
        void setThread(int ind);

        // MEMBER VARIABLES
        // �����-������
        TypePool typePool;                                    // Type of queue used / ��� ������������ �������
        std::vector<component::SingThread> threads;           // Collection of worker threads / ��������� ������� �������
        std::unique_ptr<component::QueueMutex<std::function<void(int id)>*>> queue; // Task queue / ������� �����

        std::atomic<bool> isDone;     // Flag indicating completion / ���� ���������� ������
        std::atomic<bool> isStop;     // Flag indicating immediate stop / ���� ����������� ���������
        std::atomic<int> numWaiting;  // Number of waiting threads / ���������� ��������� �������

        std::mutex mutex;             // Mutex for synchronization / ������� ��� �������������
        std::condition_variable cv;   // Condition variable for task notification / �������� ���������� ��� �����������
    };
}

#endif // THREAD_POOL_H