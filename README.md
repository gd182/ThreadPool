# ThreadPool

Библиотека пула потоков с поддержкой асинхронного выполнения задач, динамическим управлением, приоритетами и потокобезопасностью.

## Оглавление

- [Особенности](#особенности)
- [Быстрый старт](#быстрый-старт)
- [API Документация](#api-документация)
- [Примеры использования](#примеры-использования)
- [Обработка исключений](#обработка-исключений)
- [Производительность](#производительность)
- [Лицензия](#лицензия)

## Особенности

- **Потокобезопасность** - полная защита мьютексами
- **Динамическое управление** - изменение размера пула во время выполнения
- **Асинхронные задачи** - поддержка `std::future` для получения результатов
- **Приоритеты задач** - поддержка очередей с приоритетами
- **Обработка исключений** - исключения в задачах не крашат пул
- **Мониторинг** - отслеживание количества бездействующих потоков
- **Управление памятью** - автоматическая очистка ресурсов
- **Гибкость** - поддержка функций с параметрами и без

## Быстрый старт

### Простейший пример

```cpp
#include "ThreadPool.h"
#include <iostream>

void simple_task(int thread_id) {
    std::cout << "Thread " << thread_id << " is working!" << std::endl;
}

int main() {
    // Создаем пул из 4 потоков с обычной очередью
    tp::ThreadPool pool(4, tp::ThreadPool::TypePool::Normal);
    
    // Добавляем задачи
    for (int i = 0; i < 10; ++i) {
        pool.push(simple_task);
    }
    
    // Плавное завершение
    pool.stop(true);
    return 0;
}
```

### Пример с возвращаемыми значениями

```cpp
#include "ThreadPool.h"
#include <iostream>
#include <future>

double calculate_power(int thread_id, double base, double exponent) {
    return std::pow(base, exponent);
}

int main() {
    tp::ThreadPool pool(4);
    
    std::vector<std::future<double>> futures;
    for (int i = 0; i < 5; ++i) {
        auto future = pool.push(calculate_power, 2.0, static_cast<double>(i));
        futures.push_back(std::move(future));
    }
    
    // Получаем результаты
    for (size_t i = 0; i < futures.size(); ++i) {
        double result = futures[i].get();
        std::cout << "2^" << i << " = " << result << std::endl;
    }
    
    pool.stop(true);
    return 0;
}
```

### Пример с приоритетами задач

```cpp
#include "ThreadPool.h"
#include <iostream>

void high_priority_task(int thread_id) {
    std::cout << "High priority task executed by thread " << thread_id << std::endl;
}

void normal_priority_task(int thread_id) {
    std::cout << "Normal priority task executed by thread " << thread_id << std::endl;
}

void low_priority_task(int thread_id) {
    std::cout << "Low priority task executed by thread " << thread_id << std::endl;
}

int main() {
    // Создаем пул с очередью приоритетов
    tp::ThreadPool pool(2, tp::ThreadPool::TypePool::Priority);
    
    // Добавляем задачи с разными приоритетами
    pool.push(10, high_priority_task);    // Высокий приоритет
    pool.push(0, normal_priority_task);   // Обычный приоритет
    pool.push(-10, low_priority_task);    // Низкий приоритет
    
    // Даем время на выполнение
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    pool.stop(true);
    return 0;
}
```

## API Документация

### Основные методы

#### Конструкторы
```cpp
tp::ThreadPool();                          // Пул с количеством потоков по умолчанию и обычной очередью
tp::ThreadPool(TypePool typePool);         // Пул с указанным типом очереди
tp::ThreadPool(unsigned int countThreads, TypePool typePool = TypePool::Normal);
```

#### Управление пулом
```cpp
void resize(unsigned int countThreads);    // Изменить размер пула
void stop(bool isWait = false);            // Остановить пул (true - плавно)
void clearQueue();                         // Очистить очередь задач
int size();                                // Получить текущий размер пула
int numIdle();                             // Получить количество бездействующих потоков
TypePool getQueueType() const;             // Получить тип очереди
bool isRunning() const;                    // Проверить, работает ли пул
bool isStopped() const;                    // Проверить, остановлен ли пул
```

#### Добавление задач
```cpp
// Для функций с параметрами (обычный приоритет)
template<typename F, typename... Rest>
auto push(F&& f, Rest&&... rest) -> std::future<decltype(f(0, rest...))>;

// Для функций без параметров (обычный приоритет)
template<typename F>
auto push(F&& f) -> std::future<decltype(f(0))>;

// Для функций с приоритетом и параметрами
template<typename F, typename... Rest>
auto push(int priority, F&& f, Rest&&... rest) -> std::future<decltype(f(0, rest...))>;
```

#### Вспомогательные методы
```cpp
std::function<void(int)> pop();            // Извлечь задачу из очереди
std::thread& getThread(int i);            // Получить ссылку на поток (с осторожностью!)
```

## Примеры использования

### Параллельная обработка данных

```cpp
#include "ThreadPool.h"
#include <vector>
#include <algorithm>

void process_chunk(int thread_id, std::vector<int>& data, int start, int end) {
    for (int i = start; i < end; ++i) {
        data[i] = data[i] * 2 + 1; // Какая-то обработка
    }
}

int main() {
    std::vector<int> data(10000);
    tp::ThreadPool pool(std::thread::hardware_concurrency());
    
    const int chunk_size = data.size() / pool.size();
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < pool.size(); ++i) {
        int start = i * chunk_size;
        int end = (i == pool.size() - 1) ? data.size() : (i + 1) * chunk_size;
        
        futures.push_back(
            pool.push(process_chunk, std::ref(data), start, end)
        );
    }
    
    // Ждем завершения всех задач
    for (auto& future : futures) {
        future.get();
    }
    
    pool.stop(true);
    return 0;
}
```

### Динамическое управление пулом

```cpp
#include "ThreadPool.h"
#include <chrono>

int main() {
    tp::ThreadPool pool(2);
    
    // Начинаем с малого пула
    for (int i = 0; i < 10; ++i) {
        pool.push([](int id) { 
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
        });
    }
    
    // Увеличиваем пул для большей нагрузки
    pool.resize(8);
    
    // Добавляем больше задач
    for (int i = 0; i < 20; ++i) {
        pool.push([](int id) { 
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
        });
    }
    
    // Уменьшаем пул после пиковой нагрузки
    pool.resize(4);
    
    pool.stop(true);
    return 0;
}
```

### Использование приоритетов для критических задач

```cpp
#include "ThreadPool.h"
#include <iostream>

int main() {
    // Создаем пул с поддержкой приоритетов
    tp::ThreadPool pool(2, tp::ThreadPool::TypePool::Priority);
    
    // Критическая задача (высокий приоритет)
    pool.push(100, [](int id) {
        std::cout << "CRITICAL: Processing urgent task in thread " << id << std::endl;
    });
    
    // Обычные задачи
    for (int i = 0; i < 5; ++i) {
        pool.push(0, [](int id) {
            std::cout << "NORMAL: Processing task in thread " << id << std::endl;
        });
    }
    
    // Фоновая задача (низкий приоритет)
    pool.push(-50, [](int id) {
        std::cout << "BACKGROUND: Processing low priority task in thread " << id << std::endl;
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    pool.stop(true);
    return 0;
}
```

### Использование в вашем проекте

1. Скопируйте файлы в ваш проект:
   - `QueueMutex.h`
   - `ThreadPool.h` 
   - `ThreadPool.cpp`

2. Добавьте в ваш CMakeLists.txt:
```cmake
add_executable(your_project main.cpp ThreadPool.cpp)
target_compile_features(your_project PRIVATE cxx_std_11)
target_link_libraries(your_project PRIVATE pthread)
```

## Обработка исключений

Библиотека корректно обрабатывает исключения в задачах:

```cpp
try {
    auto future = pool.push([](int id) {
        throw std::runtime_error("Something went wrong!");
    });
    future.get(); // Исключение будет переброшено здесь
} catch (const std::exception& e) {
    std::cerr << "Caught exception: " << e.what() << std::endl;
}
```

## Производительность

### Рекомендации по использованию

1. **Размер пула**: Используйте `std::thread::hardware_concurrency()` для оптимального размера
2. **Тип очереди**: Используйте `TypePool::Priority` для задач с разными приоритетами
3. **Длительные задачи**: Избегайте очень длительных задач (разбивайте на подзадачи)
4. **Баланс нагрузки**: Следите за количеством бездействующих потоков `numIdle()`
5. **Память**: Большое количество задач может потреблять значительную память

### Бенчмарк

Пример тестирования производительности:

```cpp
#include "ThreadPool.h"
#include <chrono>
#include <iostream>

void benchmark() {
    tp::ThreadPool pool(4);
    const int tasks = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < tasks; ++i) {
        futures.push_back(pool.push([](int id) {
            // Легкая задача
            volatile int x = 0;
            for (int j = 0; j < 1000; ++j) {
                x += j;
            }
        }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Processed " << tasks << " tasks in " << duration.count() << "ms" << std::endl;
}
```

## Лицензия

Эта библиотека распространяется под лицензией MIT. Вы можете свободно использовать ее в коммерческих и некоммерческих проектах.

## Поддержка

Если у вас есть вопросы или предложения по улучшению, создайте issue в репозитории проекта.

---

**Примечание**: Эта библиотека предназначена для образовательных целей. Всегда тестируйте в вашей среде перед использованием в production.
