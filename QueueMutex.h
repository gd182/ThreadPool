#ifndef QUEUE_MUTEX_H
#define QUEUE_MUTEX_H

#include <queue>
#include <mutex>
#include <atomic>

namespace tp
{
    namespace component
    {
        /**
         * @brief Thread-safe queue implementation with mutex protection
         * @brief Потокобезопасная реализация очереди с защитой мьютексом
         *
         * @tparam T Type of elements stored in the queue
         * @tparam T Тип элементов, хранящихся в очереди
         */
        template <typename T>
        class QueueMutex
        {
        public:
            /**
             * @brief Push an element to the thread-safe queue
             * @brief Добавление элемента в потокобезопасную очередь
             *
             * @param value Element to push / Элемент для добавления
             * @return true if successful / true в случае успеха
             */
            bool push(T const& value)
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
            bool pop(T& value)
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
            bool empty()
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
    }
}

#endif // QUEUE_MUTEX_H