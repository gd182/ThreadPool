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
         * @brief Структура, представляющая отдельный поток в пуле
         */
        struct SingThread
        {
            std::unique_ptr<std::thread> thread;          // Thread object / Объект потока
            std::shared_ptr<std::atomic<bool>> isNotWorking; // Flag indicating if thread is working / Флаг работы потока
        };
    }

    /**
     * @brief Thread Pool class for managing and executing tasks concurrently
     * @brief Класс пула потоков для управления и выполнения задач параллельно
     *
     * This class provides a flexible thread pool that can dynamically resize
     * and execute tasks asynchronously with future support.
     *
     * Этот класс предоставляет гибкий пул потоков, который может динамически
     * изменять размер и выполнять задачи асинхронно с поддержкой future.
     */
    class ThreadPool
    {
    public:
        // CONSTRUCTORS & DESTRUCTOR
        // КОНСТРУКТОРЫ И ДЕСТРУКТОР

        ThreadPool();
        ThreadPool(int countThreads);
        ~ThreadPool();

        // POOL MANAGEMENT
        // УПРАВЛЕНИЕ ПУЛОМ

        void resize(int countThreads);
        void clearQueue();
        void stop(bool isWait = false);
        int size() { return threads.size(); };

        // TASK OPERATIONS
        // ОПЕРАЦИИ С ЗАДАЧАМИ

        std::function<void(int)> pop();

        /**
         * @brief Push a task with arguments to the queue
         * @brief Добавление задачи с аргументами в очередь
         *
         * @tparam F Function type / Тип функции
         * @tparam Rest Argument types / Типы аргументов
         * @return std::future for getting the result / std::future для получения результата
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
         * @brief Добавление простой задачи в очередь
         *
         * @tparam F Function type / Тип функции
         * @return std::future for getting the result / std::future для получения результата
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
        // УДАЛЕННАЯ СЕМАНТИКА КОПИРОВАНИЯ И ПЕРЕМЕЩЕНИЯ
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        /**
         * @brief Get the number of idle threads in the pool
         * @brief Получить количество бездействующих потоков в пуле
         *
         * @return int Number of threads currently waiting for tasks
         * @return int Количество потоков, ожидающих задачи в данный момент
         */
        int numIdle() { return this->numWaiting; }

        /**
         * @brief Get reference to a specific thread by index
         * @brief Получить ссылку на конкретный поток по индексу
         *
         * @param i Index of the thread (0 <= i < size())
         * @param i Индекс потока (0 <= i < size())
         * @return std::thread& Reference to the thread object
         * @return std::thread& Ссылка на объект потока
         *
         * @throw std::out_of_range if index is invalid
         * @throw std::out_of_range если индекс недействителен
         *
         * Warning: Use with caution! Direct thread manipulation can be dangerous.
         * Предупреждение: Используйте с осторожностью! Прямое управление потоками может быть опасным.
         */
        std::thread& get_thread(int i) {
            if (i < 0 || i >= threads.size()) {
                throw std::out_of_range("Thread index out of range");
            }
            return *this->threads[i].thread;
        }

    private:
        // INTERNAL METHODS
        // ВНУТРЕННИЕ МЕТОДЫ

        void init();
        void setThread(int ind);

        // MEMBER VARIABLES
        // ЧЛЕНЫ-ДАННЫЕ

        std::vector<tp::component::SingThread> threads;   // Collection of worker threads / Коллекция рабочих потоков
        tp::component::QueueMutex<std::function<void(int id)>*> queue; // Task queue / Очередь задач

        std::atomic<bool> isDone;     // Flag indicating completion / Флаг завершения работы
        std::atomic<bool> isStop;     // Flag indicating immediate stop / Флаг немедленной остановки
        std::atomic<int> numWaiting;  // Number of waiting threads / Количество ожидающих потоков

        std::mutex mutex;             // Mutex for synchronization / Мьютекс для синхронизации
        std::condition_variable cv;   // Condition variable for task notification / Условная переменная для уведомлений
    };
}

#endif // THREAD_POOL_H