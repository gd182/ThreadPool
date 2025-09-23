#ifndef QUEUE_MUTEX_H
#define QUEUE_MUTEX_H

#include <queue>
#include <mutex>
#include <atomic>
#include <functional>

namespace tp
{
    namespace component
    {
        /**
         * @brief Interface for thread-safe queue implementations
         * @brief Интерфейс для потокобезопасных реализаций очереди
         *
         * @tparam T Type of elements stored in the queue
         * @tparam T Тип элементов, хранящихся в очереди
         */
        template <typename T>
        class QueueMutex
        {
        public:
            virtual bool push(T const& value) = 0;
            virtual bool pop(T& value) = 0;
            virtual bool empty() = 0;
            virtual ~QueueMutex() = default;
        };

        /**
         * @brief Thread-safe queue implementation with mutex protection
         * @brief Потокобезопасная реализация очереди с защитой мьютексом
         *
         * @tparam T Type of elements stored in the queue
         * @tparam T Тип элементов, хранящихся в очереди
         */
        template <typename T>
        class NormalQueue : public QueueMutex<T>
        {
        public:
            /**
             * @brief Push an element to the thread-safe queue
             * @brief Добавление элемента в потокобезопасную очередь
             *
             * @param value Element to push / Элемент для добавления
             * @return true if successful / true в случае успеха
             */
            bool push(T const& value) override
            {
                // Lock mutex for thread-safe operation
                // Блокировка мьютекса для потокобезопасной операции
                std::unique_lock<std::mutex> lock(this->mutex);
                this->queue.push(value);
                return true;
            };
            /**
             * @brief Pop an element from the thread-safe queue
             * @brief Извлечение элемента из потокобезопасной очереди
             *
             * @param value Reference to store popped element / Ссылка для сохранения извлеченного элемента
             * @return true if element was popped, false if queue is empty / true если элемент извлечен, false если очередь пуста
             */
            bool pop(T& value) override
            {
                // Lock mutex for thread-safe operation
                // Блокировка мьютекса для потокобезопасной операции
                std::unique_lock<std::mutex> lock(this->mutex);

                // Check if queue is empty before popping
                // Проверка пустоты очереди перед извлечением
                if (this->queue.empty())
                    return false;

                value = this->queue.front();
                this->queue.pop();
                return true;
            };

            /**
             * @brief Check if the queue is empty
             * @brief Проверка, пуста ли очередь
             *
             * @return true if queue is empty, false otherwise / true если очередь пуста, false в противном случае
             */
            bool empty() override
            {
                // Lock mutex for thread-safe check
                // Блокировка мьютекса для потокобезопасной проверки
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->queue.empty();
            }

        private:
            std::queue<T> queue;      // Internal queue storage / Внутреннее хранилище очереди
            std::mutex mutex;         // Mutex for thread synchronization / Мьютекс для синхронизации потоков
        };
        /**
         * @brief Structure for prioritized tasks with comparison operator
         * @brief Структура для приоритетных задач с оператором сравнения
         */
        template <typename T>
        struct PrioritizedTask {
            T function;                            // Task function / Функция задачи
            int priority;                          // Task priority (higher = more important) / Приоритет задачи (выше = важнее)
            uint32_t sequence;                     // Sequence number for FIFO within same priority / Порядковый номер для FIFO в пределах одного приоритета

            /**
             * @brief Comparison operator for priority queue ordering
             * @brief Оператор сравнения для упорядочивания в очереди с приоритетом
             *
             * @param other Other task to compare with / Другая задача для сравнения
             * @return true if this task has lower priority / true если эта задача имеет меньший приоритет
             */
            bool operator<(const PrioritizedTask& other) const {
                // Higher priority tasks should come first
                // Задачи с более высоким приоритетом должны быть первыми
                if (priority != other.priority) {
                    return priority < other.priority; // Lower priority value = higher priority
                }

                // For same priority, handle sequence wrap-around
                // Для одинакового приоритета обрабатываем переполнение счетчика
                uint32_t diff = sequence - other.sequence;

                // If the difference is large (wrap-around occurred), reverse the order
                // Если разница большая (произошло переполнение), меняем порядок
                if (diff > 0x80000000) {
                    return sequence > other.sequence; // Older task (wrap-around) comes first
                }
                else {
                    return sequence < other.sequence; // Normal FIFO order
                }
            }
        };

        template <typename T>
        class PriorityQueue : public QueueMutex<T>
        {
        public:
            /**
             * @brief Push a task to the priority queue
             * @brief Добавление задачи в очередь с приоритетом
             *
             * @param value Function pointer to push / Указатель на функцию для добавления
             * @return true if successful / true в случае успеха
             */
            bool push(T const& value) override
            {
                // Default priority is 0 (normal)
                // Приоритет по умолчанию - 0 (обычный)
                return this->push(value, 0);
            };
            
            /**
             * @brief Push a task with specific priority to the queue
             * @brief Добавление задачи с определенным приоритетом в очередь
             *
             * @param value Function pointer to push / Указатель на функцию для добавления
             * @param priority Task priority (higher = more important) / Приоритет задачи (выше = важнее)
             * @return true if successful / true в случае успеха
             */
            bool push(T value, int priority)
            {
                // Lock mutex for thread-safe operation
                // Блокировка мьютекса для потокобезопасной операции
                std::unique_lock<std::mutex> lock(this->mutex);

                // Create prioritized task with sequence number
                // Создаем задачу с приоритетом и порядковым номером
                PrioritizedTask<T> task;
                task.function = value;
                task.priority = priority;
                task.sequence = nextSequence.fetch_add(1, std::memory_order_relaxed);
                if (task.sequence == UINT32_MAX) {
                    nextSequence.store(0, std::memory_order_relaxed);
                }

                this->queue.push(task);
                return true;
            };

            /**
             * @brief Pop the highest priority task from the queue
             * @brief Извлечение задачи с наивысшим приоритетом из очереди
             *
             * @param value Reference to store popped task function / Ссылка для сохранения извлеченной функции задачи
             * @return true if task was popped, false if queue is empty / true если задача извлечена, false если очередь пуста
             */
            bool pop(std::function<void(int id)>*& value) override
            {
                // Lock mutex for thread-safe operation
                // Блокировка мьютекса для потокобезопасной операции
                std::unique_lock<std::mutex> lock(this->mutex);

                // Check if queue is empty before popping
                // Проверка пустоты очереди перед извлечением
                if (this->queue.empty())
                    return false;

                // Get the highest priority task
                // Получаем задачу с наивысшим приоритетом
                PrioritizedTask<T> task = this->queue.top();
                value = task.function;
                this->queue.pop();
                return true;
            };

            /**
             * @brief Check if the queue is empty
             * @brief Проверка, пуста ли очередь
             *
             * @return true if queue is empty, false otherwise / true если очередь пуста, false в противном случае
             */
            bool empty() override
            {
                // Lock mutex for thread-safe check
                // Блокировка мьютекса для потокобезопасной проверки
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->queue.empty();
            }

        private:
            std::priority_queue<PrioritizedTask<T>> queue; // Internal priority queue storage / Внутреннее хранилище очереди с приоритетом
            std::mutex mutex;                           // Mutex for thread synchronization / Мьютекс для синхронизации потоков
            std::atomic<uint32_t> nextSequence{ 0 };      // Sequence counter for FIFO ordering / Счетчик порядка для FIFO упорядочивания
        };
    }
}

#endif // QUEUE_MUTEX_H