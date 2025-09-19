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
         * @brief ���������������� ���������� ������� � ������� ���������
         *
         * @tparam T Type of elements stored in the queue
         * @tparam T ��� ���������, ���������� � �������
         */
        template <typename T>
        class QueueMutex
        {
        public:
            /**
             * @brief Push an element to the thread-safe queue
             * @brief ���������� �������� � ���������������� �������
             *
             * @param value Element to push / ������� ��� ����������
             * @return true if successful / true � ������ ������
             */
            bool push(T const& value)
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
            bool pop(T& value)
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
            bool empty()
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
    }
}

#endif // QUEUE_MUTEX_H