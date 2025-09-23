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
         * @brief ��������� ��� ���������������� ���������� �������
         *
         * @tparam T Type of elements stored in the queue
         * @tparam T ��� ���������, ���������� � �������
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
         * @brief ���������������� ���������� ������� � ������� ���������
         *
         * @tparam T Type of elements stored in the queue
         * @tparam T ��� ���������, ���������� � �������
         */
        template <typename T>
        class NormalQueue : public QueueMutex<T>
        {
        public:
            /**
             * @brief Push an element to the thread-safe queue
             * @brief ���������� �������� � ���������������� �������
             *
             * @param value Element to push / ������� ��� ����������
             * @return true if successful / true � ������ ������
             */
            bool push(T const& value) override
            {
                // Lock mutex for thread-safe operation
                // ���������� �������� ��� ���������������� ��������
                std::unique_lock<std::mutex> lock(this->mutex);
                this->queue.push(value);
                return true;
            };
            /**
             * @brief Pop an element from the thread-safe queue
             * @brief ���������� �������� �� ���������������� �������
             *
             * @param value Reference to store popped element / ������ ��� ���������� ������������ ��������
             * @return true if element was popped, false if queue is empty / true ���� ������� ��������, false ���� ������� �����
             */
            bool pop(T& value) override
            {
                // Lock mutex for thread-safe operation
                // ���������� �������� ��� ���������������� ��������
                std::unique_lock<std::mutex> lock(this->mutex);

                // Check if queue is empty before popping
                // �������� ������� ������� ����� �����������
                if (this->queue.empty())
                    return false;

                value = this->queue.front();
                this->queue.pop();
                return true;
            };

            /**
             * @brief Check if the queue is empty
             * @brief ��������, ����� �� �������
             *
             * @return true if queue is empty, false otherwise / true ���� ������� �����, false � ��������� ������
             */
            bool empty() override
            {
                // Lock mutex for thread-safe check
                // ���������� �������� ��� ���������������� ��������
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->queue.empty();
            }

        private:
            std::queue<T> queue;      // Internal queue storage / ���������� ��������� �������
            std::mutex mutex;         // Mutex for thread synchronization / ������� ��� ������������� �������
        };
        /**
         * @brief Structure for prioritized tasks with comparison operator
         * @brief ��������� ��� ������������ ����� � ���������� ���������
         */
        template <typename T>
        struct PrioritizedTask {
            T function;                            // Task function / ������� ������
            int priority;                          // Task priority (higher = more important) / ��������� ������ (���� = ������)
            uint32_t sequence;                     // Sequence number for FIFO within same priority / ���������� ����� ��� FIFO � �������� ������ ����������

            /**
             * @brief Comparison operator for priority queue ordering
             * @brief �������� ��������� ��� �������������� � ������� � �����������
             *
             * @param other Other task to compare with / ������ ������ ��� ���������
             * @return true if this task has lower priority / true ���� ��� ������ ����� ������� ���������
             */
            bool operator<(const PrioritizedTask& other) const {
                // Higher priority tasks should come first
                // ������ � ����� ������� ����������� ������ ���� �������
                if (priority != other.priority) {
                    return priority < other.priority; // Lower priority value = higher priority
                }

                // For same priority, handle sequence wrap-around
                // ��� ����������� ���������� ������������ ������������ ��������
                uint32_t diff = sequence - other.sequence;

                // If the difference is large (wrap-around occurred), reverse the order
                // ���� ������� ������� (��������� ������������), ������ �������
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
             * @brief ���������� ������ � ������� � �����������
             *
             * @param value Function pointer to push / ��������� �� ������� ��� ����������
             * @return true if successful / true � ������ ������
             */
            bool push(T const& value) override
            {
                // Default priority is 0 (normal)
                // ��������� �� ��������� - 0 (�������)
                return this->push(value, 0);
            };
            
            /**
             * @brief Push a task with specific priority to the queue
             * @brief ���������� ������ � ������������ ����������� � �������
             *
             * @param value Function pointer to push / ��������� �� ������� ��� ����������
             * @param priority Task priority (higher = more important) / ��������� ������ (���� = ������)
             * @return true if successful / true � ������ ������
             */
            bool push(T value, int priority)
            {
                // Lock mutex for thread-safe operation
                // ���������� �������� ��� ���������������� ��������
                std::unique_lock<std::mutex> lock(this->mutex);

                // Create prioritized task with sequence number
                // ������� ������ � ����������� � ���������� �������
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
             * @brief ���������� ������ � ��������� ����������� �� �������
             *
             * @param value Reference to store popped task function / ������ ��� ���������� ����������� ������� ������
             * @return true if task was popped, false if queue is empty / true ���� ������ ���������, false ���� ������� �����
             */
            bool pop(std::function<void(int id)>*& value) override
            {
                // Lock mutex for thread-safe operation
                // ���������� �������� ��� ���������������� ��������
                std::unique_lock<std::mutex> lock(this->mutex);

                // Check if queue is empty before popping
                // �������� ������� ������� ����� �����������
                if (this->queue.empty())
                    return false;

                // Get the highest priority task
                // �������� ������ � ��������� �����������
                PrioritizedTask<T> task = this->queue.top();
                value = task.function;
                this->queue.pop();
                return true;
            };

            /**
             * @brief Check if the queue is empty
             * @brief ��������, ����� �� �������
             *
             * @return true if queue is empty, false otherwise / true ���� ������� �����, false � ��������� ������
             */
            bool empty() override
            {
                // Lock mutex for thread-safe check
                // ���������� �������� ��� ���������������� ��������
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->queue.empty();
            }

        private:
            std::priority_queue<PrioritizedTask<T>> queue; // Internal priority queue storage / ���������� ��������� ������� � �����������
            std::mutex mutex;                           // Mutex for thread synchronization / ������� ��� ������������� �������
            std::atomic<uint32_t> nextSequence{ 0 };      // Sequence counter for FIFO ordering / ������� ������� ��� FIFO ��������������
        };
    }
}

#endif // QUEUE_MUTEX_H