#include "ThreadPool.h"
#include <iostream>

// CONSTRUCTORS & DESTRUCTOR
// КОНСТРУКТОРЫ И ДЕСТРУКТОР

tp::ThreadPool::ThreadPool(TypePool typePool)
{
    init(typePool);

    // Use hardware concurrency as default thread count
    // Используем аппаратную параллельность как количество потоков по умолчанию
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
        numThreads = 1; // Fallback if hardware_concurrency returns 0 / Резервный вариант если hardware_concurrency возвращает 0
    }

    threads.resize(numThreads);

    // Create and configure worker threads
    // Создание и настройка рабочих потоков
    for (int i = 0; i < numThreads; ++i)
    {
        threads[i].isNotWorking = std::make_shared<std::atomic<bool>>(false);
        setThread(i);
    }
}

tp::ThreadPool::ThreadPool(unsigned int numThreads, TypePool typePool)
{
    // Initialize pool state
    // Инициализация состояния пула
    init(typePool);
    threads.resize(numThreads);

    // Create and configure worker threads
    // Создание и настройка рабочих потоков
    for (int i = 0; i < numThreads; ++i)
    {
        threads[i].isNotWorking = std::make_shared<std::atomic<bool>>(false);
        setThread(i);
    }
}

tp::ThreadPool::~ThreadPool()
{
    // Graceful shutdown - wait for tasks to complete
    // Плавное завершение - ожидание завершения задач
    stop(true);
}

// POOL MANAGEMENT METHODS
// МЕТОДЫ УПРАВЛЕНИЯ ПУЛОМ

void tp::ThreadPool::stop(bool isWait)
{
    if (!isWait) {
        // Immediate stop - clear queue and force threads to stop
        // Немедленная остановка - очистка очереди и принудительная остановка потоков
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
        // Плавная остановка - позволить задачам завершиться естественно
        if (isDone || isStop)
            return;
        isDone = true;
    }

    {
        // Notify all waiting threads to wake up and check conditions
        // Уведомление всех ожидающих потоков для проверки условий
        std::unique_lock<std::mutex> lock(this->mutex);
        cv.notify_all();
    }

    // Wait for all threads to finish execution
    // Ожидание завершения выполнения всех потоков
    for (int i = 0, n = size(); i < n; ++i) {
        if (threads[i].thread && threads[i].thread->joinable())
            threads[i].thread->join();
    }

    // Cleanup resources
    // Очистка ресурсов
    clearQueue();
    threads.clear();
}

void tp::ThreadPool::resize(unsigned int numThreads)
{
    if (!isStop && !isDone) {
        int oldNumThread = threads.size();

        if (oldNumThread <= numThreads) {
            // Increase thread count - add new worker threads
            // Увеличение количества потоков - добавление новых рабочих потоков
            threads.resize(numThreads);
            for (int i = oldNumThread; i < numThreads; ++i)
            {
                threads[i].isNotWorking = std::make_shared<std::atomic<bool>>(false);
                setThread(i);
            }
        }
        else {
            // Decrease thread count - stop excess threads
            // Уменьшение количества потоков - остановка лишних потоков
            for (int i = oldNumThread - 1; i >= numThreads; --i)
            {
                *threads[i].isNotWorking = true;
                threads[i].thread->detach();
            }

            // Notify detached threads to stop
            // Уведомление отсоединенных потоков об остановке
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
    // Удаление всех ожидающих задач для предотвращения утечек памяти
    while (queue->pop(fun))
        delete fun;
}

// INTERNAL METHODS
// ВНУТРЕННИЕ МЕТОДЫ

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
    // Лямбда-функция, представляющая жизненный цикл рабочего потока
    auto f = [this, ind, flag]() {
        std::atomic<bool>& _flag = *flag;
        std::function<void(int id)>* fun;
        bool isPop = queue->pop(fun);

        // Main worker thread loop
        // Основной цикл рабочего потока
        while (true) {
            // Process available tasks from the queue
            // Обработка доступных задач из очереди
            while (isPop) {
                // Smart pointer for automatic memory management
                // Умный указатель для автоматического управления памятью
                std::unique_ptr<std::function<void(int id)>> func(fun);

                try {
                    // Execute the task with thread ID
                    // Выполнение задачи с идентификатором потока
                    (*fun)(ind);
                }
                catch (const std::exception& e) {
                    // Handle standard exceptions gracefully
                    // Обработка стандартных исключений
                    std::cerr << "Exception in thread " << ind << ": " << e.what() << std::endl;
                }
                catch (...) {
                    // Handle unknown exceptions
                    // Обработка неизвестных исключений
                    std::cerr << "Unknown exception in thread " << ind << std::endl;
                }

                if (_flag)
                    return;  // Exit if thread should stop
                else
                    isPop = queue->pop(fun);
            }

            // Wait for new tasks when queue is empty
            // Ожидание новых задач при пустой очереди
            std::unique_lock<std::mutex> lock(mutex);
            ++numWaiting;

            // Wait for notification or condition change
            // Ожидание уведомления или изменения условия
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
    // Создание и запуск рабочего потока
    threads[ind].thread.reset(new std::thread(f));
}

// TASK OPERATIONS
// ОПЕРАЦИИ С ЗАДАЧАМИ

std::function<void(int)> tp::ThreadPool::pop()
{
    std::function<void(int id)>* fun = nullptr;
    queue->pop(fun);

    // Smart pointer for automatic cleanup
    // Умный указатель для автоматической очистки
    std::unique_ptr<std::function<void(int id)>> func(fun);
    std::function<void(int)> f;

    if (fun)
        f = *fun;

    return f;
}