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
        /**
         * @brief Type of thread pool queue
         * @brief Тип очереди пула потоков
         */
        enum class TypePool
        {
            Normal,   // Normal FIFO queue / Обычная очередь FIFO
            Priority  // Priority-based queue / Очередь на основе приоритетов
        };

        // CONSTRUCTORS & DESTRUCTOR
        // КОНСТРУКТОРЫ И ДЕСТРУКТОР

        /**
         * @brief Constructor with default number of threads
         * @brief Конструктор с количеством потоков по умолчанию
         *
         * @param typePool Type of queue to use / Тип используемой очереди
         */
        ThreadPool(TypePool typePool = TypePool::Normal);
        
        /**
         * @brief Constructor with specified number of threads
         * @brief Конструктор с указанным количеством потоков
         *
         * @param numThreads Number of worker threads / Количество рабочих потоков
         * @param typePool Type of queue to use / Тип используемой очереди
         */
        ThreadPool(unsigned int countThreads, TypePool typePool = TypePool::Normal);
        
        /**
         * @brief Destructor - automatically stops the pool
         * @brief Деструктор - автоматически останавливает пул
         */
        ~ThreadPool();

        // POOL MANAGEMENT
// УПРАВЛЕНИЕ ПУЛОМ

/**
 * @brief Resize the thread pool to the specified number of threads
 * @brief Изменяет размер пула потоков до указанного количества потоков
 *
 * @param countThreads New number of threads in the pool / Новое количество потоков в пуле
 *
 * @note If increasing size, new threads will be created immediately
 * @note При увеличении размера новые потоки будут созданы немедленно
 *
 * @note If decreasing size, excess threads will be stopped after completing current tasks
 * @note При уменьшении размера лишние потоки будут остановлены после завершения текущих задач
 *
 * @warning Does nothing if pool is stopped or stopping
 * @warning Ничего не делает, если пул остановлен или останавливается
 */
        void resize(unsigned int countThreads);

        /**
         * @brief Clear all pending tasks from the queue
         * @brief Очищает все ожидающие задачи из очереди
         *
         * @note This will delete all tasks that have not yet been executed
         * @note Это удалит все задачи, которые еще не были выполнены
         *
         * @warning Tasks being executed by threads are not affected
         * @warning Задачи, выполняемые потоками в данный момент, не затрагиваются
         *
         * @warning Memory of task objects will be freed
         * @warning Память объектов задач будет освобождена
         */
        void clearQueue();

        /**
         * @brief Stop the thread pool
         * @brief Останавливает пул потоков
         *
         * @param isWait If true, wait for current tasks to complete (graceful shutdown)
         *               If false, stop immediately (force shutdown)
         * @param isWait Если true, ждет завершения текущих задач (плавная остановка)
         *               Если false, останавливает немедленно (принудительная остановка)
         *
         * @note After stopping, the pool cannot be restarted
         * @note После остановки пул нельзя перезапустить
         *
         * @warning With isWait=false, queued tasks will be discarded without execution
         * @warning При isWait=false задачи в очереди будут отброшены без выполнения
         */
        void stop(bool isWait = false);

        /**
         * @brief Get the current number of threads in the pool
         * @brief Получает текущее количество потоков в пуле
         *
         * @return int Number of worker threads / Количество рабочих потоков
         *
         * @note This includes both active and idle threads
         * @note Включает как активные, так и бездействующие потоки
         */
        int size() { return threads.size(); };

        // TASK OPERATIONS
        // ОПЕРАЦИИ С ЗАДАЧАМИ

        /**
         * @brief Pop a task from the front of the queue
         * @brief Извлекает задачу из начала очереди
         *
         * @return std::function<void(int)> The popped task, or empty function if queue was empty
         * @return std::function<void(int)> Извлеченная задача или пустая функция, если очередь пуста
         *
         * @note The task is removed from the queue
         * @note Задача удаляется из очереди
         *
         * @warning This operation is thread-safe but should be used with caution
         * @warning Эта операция потокобезопасна, но должна использоваться с осторожностью
         *
         * @warning If no task is available, returns an empty std::function
         * @warning Если задач нет, возвращает пустую std::function
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
         * @brief Добавление задачи с аргументами в очередь
         *
         * @tparam F Function type / Тип функции
         * @tparam Rest Argument types / Типы аргументов
         * @return std::future for getting the result / std::future для получения результата
         */
        template<typename F, typename... Rest>
        auto push(F&& f, Rest&&... rest) -> std::future<decltype(f(0, rest...))>
        {
            return this->push(0, std::forward<F>(f), std::forward<Rest>(rest)...);
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
            return this->push(0, std::forward<F>(f));
        };

        /**
         * @brief Push a task with priority and arguments to the queue
         * @brief Добавление задачи с приоритетом и аргументами в очередь
         *
         * @tparam F Function type / Тип функции
         * @tparam Rest Argument types / Типы аргументов
         * @param priority Task priority (higher = more important) / Приоритет задачи (выше = важнее)
         * @return std::future for getting the result / std::future для получения результата
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
            // Используем очередь с приоритетом если доступно, иначе обычное добавление
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
        std::thread& getThread(int i);

        /**
         * @brief Get the type of queue used by the pool
         * @brief Получить тип очереди, используемой пулом
         *
         * @return TypePool Type of the queue / Тип очереди
         */
        TypePool getQueueType() const { return typePool; }

        /**
         * @brief Check if the thread pool is currently running and accepting tasks
         * @brief Проверяет, работает ли пул потоков в данный момент и принимает ли задачи
         *
         * @return true if the pool is running and can accept new tasks
         * @return true если пул работает и может принимать новые задачи
         *
         * @return false if the pool is stopped or in the process of stopping
         * @return false если пул остановлен или находится в процессе остановки
         */
        bool isRunning() const { return !isDone && !isStop; }

        /**
         * @brief Check if the thread pool has been stopped or is stopping
         * @brief Проверяет, был ли пул потоков остановлен или находится в процессе остановки
         *
         * @return true if the pool is stopped or in the process of stopping
         * @return true если пул остановлен или находится в процессе остановки
         *
         * @return false if the pool is running and accepting tasks
         * @return false если пул работает и принимает задачи
         *
         * @note This is the inverse of isRunning()
         * @note Это обратная функция к isRunning()
         */
        bool isStopped() const { return isDone || isStop; }
    private:
        // INTERNAL METHODS
        // ВНУТРЕННИЕ МЕТОДЫ

        void init(TypePool typePool);
        void setThread(int ind);

        // MEMBER VARIABLES
        // ЧЛЕНЫ-ДАННЫЕ
        TypePool typePool;                                    // Type of queue used / Тип используемой очереди
        std::vector<component::SingThread> threads;           // Collection of worker threads / Коллекция рабочих потоков
        std::unique_ptr<component::QueueMutex<std::function<void(int id)>*>> queue; // Task queue / Очередь задач

        std::atomic<bool> isDone;     // Flag indicating completion / Флаг завершения работы
        std::atomic<bool> isStop;     // Flag indicating immediate stop / Флаг немедленной остановки
        std::atomic<int> numWaiting;  // Number of waiting threads / Количество ожидающих потоков

        std::mutex mutex;             // Mutex for synchronization / Мьютекс для синхронизации
        std::condition_variable cv;   // Condition variable for task notification / Условная переменная для уведомлений
    };
}

#endif // THREAD_POOL_H