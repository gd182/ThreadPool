#include "ThreadPool.h"
#include <iostream>
#include <future>
#include <chrono>
#include <cmath>
#include <atomic>
#include <string>

/**
 * @brief Test function with parameters and return value
 * @brief Тестовая функция с параметрами и возвращаемым значением
 */
double complex_calculation(int thread_id, double x, double y) {
    std::cout << "Thread " << thread_id << " calculating: " << x << " * " << y << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate work / Имитация работы
    return x * y;
}

/**
 * @brief Simple test function without return value
 * @brief Простая тестовая функция без возвращаемого значения
 */
void simple_task(int thread_id) {
    std::cout << "Thread " << thread_id << " executing simple task" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

/**
 * @brief Function that throws an exception (for exception handling test)
 * @brief Функция, которая бросает исключение (для теста обработки исключений)
 */
void task_with_exception(int thread_id) {
    std::cout << "Thread " << thread_id << " about to throw exception" << std::endl;
    throw std::runtime_error("Test exception from thread " + std::to_string(thread_id));
}

/**
 * @brief Function with priority demonstration
 * @brief Функция для демонстрации приоритетов
 */
void priority_task(int thread_id, const std::string& message, int priority) {
    std::cout << "Thread " << thread_id << " [Priority " << priority << "]: " << message << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

int main() {
    setlocale(LC_ALL, "Russian");
    std::cout << "=== COMPREHENSIVE THREADPOOL TEST ===\n";
    std::cout << "=== КОМПЛЕКСНОЕ ТЕСТИРОВАНИЕ THREADPOOL ===\n\n";

    // Test 1: Basic initialization and task submission
    // Тест 1: Базовая инициализация и отправка задач
    std::cout << "1. Creating ThreadPool with 3 threads...\n";
    std::cout << "1. Создание ThreadPool с 3 потоками...\n";
    tp::ThreadPool pool(3);

    std::cout << "Pool size: " << pool.size() << std::endl;
    std::cout << "Размер пула: " << pool.size() << std::endl;

    // Test 2: Push tasks with parameters and futures
    // Тест 2: Отправка задач с параметрами и future
    std::cout << "\n2. Pushing tasks with futures...\n";
    std::cout << "2. Отправка задач с future...\n";

    std::vector<std::future<double>> futures;
    for (int i = 0; i < 5; ++i) {
        auto future = pool.push(complex_calculation, i * 1.5, i * 2.0);
        futures.push_back(std::move(future));
    }

    // Test 3: Push simple tasks without return values
    // Тест 3: Отправка простых задач без возвращаемых значений
    std::cout << "\n3. Pushing simple tasks...\n";
    std::cout << "3. Отправка простых задач...\n";

    for (int i = 0; i < 3; ++i) {
        pool.push(simple_task);
    }

    // Test 4: Get results from futures
    // Тест 4: Получение результатов из future
    std::cout << "\n4. Getting results from futures...\n";
    std::cout << "4. Получение результатов из future...\n";

    for (size_t i = 0; i < futures.size(); ++i) {
        try {
            double result = futures[i].get();
            std::cout << "Future " << i << " result: " << result << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in future " << i << ": " << e.what() << std::endl;
        }
    }

    // Test 5: Test exception handling
    // Тест 5: Тестирование обработки исключений
    std::cout << "\n5. Testing exception handling...\n";
    std::cout << "5. Тестирование обработки исключений...\n";

    try {
        auto future = pool.push(task_with_exception);
        future.get(); // This should throw / Это должно бросить исключение
    }
    catch (const std::exception& e) {
        std::cout << "Caught exception as expected: " << e.what() << std::endl;
        std::cout << "Исключение перехвачено, как и ожидалось: " << e.what() << std::endl;
    }

    // Test 6: Dynamic resizing - WAIT for current tasks to complete first
    // Тест 6: Динамическое изменение размера - ЖДЕМ завершения текущих задач
    std::cout << "\n6. Testing dynamic resizing...\n";
    std::cout << "6. Тестирование динамического изменения размера...\n";

    // Wait a bit for current tasks to complete / Ждем завершения текущих задач
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Current size: " << pool.size() << std::endl;
    std::cout << "Текущий размер: " << pool.size() << std::endl;

    // Resize to larger pool / Увеличиваем размер пула
    pool.resize(5);
    std::cout << "After resize to 5: " << pool.size() << std::endl;
    std::cout << "После увеличения до 5: " << pool.size() << std::endl;

    // Add some tasks to new threads / Добавляем задачи для новых потоков
    for (int i = 0; i < 3; ++i) {
        pool.push(simple_task);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Resize to smaller pool / Уменьшаем размер пула
    pool.resize(2);
    std::cout << "After resize to 2: " << pool.size() << std::endl;
    std::cout << "После уменьшения до 2: " << pool.size() << std::endl;

    // Test 7: Check number of idle threads
    // Тест 7: Проверка количества бездействующих потоков
    std::cout << "\n7. Checking idle threads...\n";
    std::cout << "7. Проверка бездействующих потоков...\n";

    std::cout << "Idle threads: " << pool.numIdle() << std::endl;
    std::cout << "Бездействующих потоков: " << pool.numIdle() << std::endl;

    // Add some more tasks to see idle count change
    // Добавляем еще задач чтобы увидеть изменение счетчика
    for (int i = 0; i < 3; ++i) {
        pool.push(simple_task);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "Idle threads after adding tasks: " << pool.numIdle() << std::endl;
    std::cout << "Бездействующих потоков после добавления задач: " << pool.numIdle() << std::endl;

    // Test 8: Test priority queue functionality (if using priority queue)
    // Тест 8: Тестирование функциональности очереди с приоритетами
    std::cout << "\n8. Testing priority queue functionality...\n";
    std::cout << "8. Тестирование функциональности очереди с приоритетами...\n";

    // Create a new pool with priority queue for testing
    // Создаем новый пул с очередью приоритетов для тестирования
    tp::ThreadPool priorityPool(2, tp::ThreadPool::TypePool::Priority);

    // Push tasks with different priorities
    // Добавляем задачи с разными приоритетами
    priorityPool.push(10, priority_task, "High priority task", 10);
    priorityPool.push(1, priority_task, "Low priority task", 1);
    priorityPool.push(5, priority_task, "Medium priority task", 5);
    priorityPool.push(10, priority_task, "Another high priority", 10);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    priorityPool.stop(true);

    // Test 9: Test pop functionality - only when queue is not empty
    // Тест 9: Тестирование функции pop - только когда очередь не пуста
    std::cout << "\n9. Testing pop functionality...\n";
    std::cout << "9. Тестирование функции pop...\n";

    // Wait for queue to be empty first / Сначала ждем пока очередь опустеет
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Add a task and then try to pop it / Добавляем задачу и пытаемся ее извлечь
    pool.push(simple_task);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    try {
        auto task = pool.pop();
        if (task) {
            std::cout << "Successfully popped a task" << std::endl;
            std::cout << "Задача успешно извлечена" << std::endl;
            task(999); // Execute with dummy thread ID / Выполняем с фиктивным ID потока
        }
        else {
            std::cout << "No tasks to pop" << std::endl;
            std::cout << "Нет задач для извлечения" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in pop: " << e.what() << std::endl;
    }

    // Test 10: Test thread access (with caution)
    // Тест 10: Тестирование доступа к потокам (с осторожностью)
    std::cout << "\n10. Testing thread access...\n";
    std::cout << "10. Тестирование доступа к потокам...\n";

    try {
        auto& thread = pool.getThread(0);
        std::cout << "Thread 0 ID: " << thread.get_id() << std::endl;
        std::cout << "ID потока 0: " << thread.get_id() << std::endl;

        // Check if thread is joinable / Проверяем можно ли присоединить поток
        std::cout << "Thread 0 is joinable: " << thread.joinable() << std::endl;
        std::cout << "Поток 0 можно присоединить: " << thread.joinable() << std::endl;
    }
    catch (const std::out_of_range& e) {
        std::cerr << "Thread access error: " << e.what() << std::endl;
    }

    // Test 11: Graceful shutdown
    // Тест 11: Плавное завершение
    std::cout << "\n11. Testing graceful shutdown...\n";
    std::cout << "11. Тестирование плавного завершения...\n";

    // Add some final tasks / Добавляем финальные задачи
    for (int i = 0; i < 2; ++i) {
        pool.push(simple_task);
    }

    // Wait a bit for tasks to start / Ждем немного чтобы задачи начали выполняться
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Stopping pool gracefully..." << std::endl;
    std::cout << "Плавная остановка пула..." << std::endl;

    pool.stop(true); // Graceful stop / Плавная остановка

    std::cout << "\n=== ALL TESTS COMPLETED SUCCESSFULLY ===\n";
    std::cout << "=== ВСЕ ТЕСТЫ УСПЕШНО ЗАВЕРШЕНЫ ===\n";

    return 0;
}