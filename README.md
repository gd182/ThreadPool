# ThreadPool

Библиотека пула потоков с поддержкой асинхронного выполнения задач, динамическим управлением и потокобезопасностью.

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
    // Создаем пул из 4 потоков
    tp::ThreadPool pool(4);
    
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

## API Документация

### Основные методы

#### Конструкторы
```cpp
tp::ThreadPool();                          // Пул с количеством потоков по умолчанию
tp::ThreadPool(int countThreads);          // Пул с указанным количеством потоков
```

#### Управление пулом
```cpp
void resize(int countThreads);             // Изменить размер пула
void stop(bool isWait = false);            // Остановить пул (true - плавно)
void clearQueue();                         // Очистить очередь задач
int size();                                // Получить текущий размер пула
int numIdle();                             // Получить количество бездействующих потоков
```

#### Добавление задач
```cpp
// Для функций с параметрами
template<typename F, typename... Rest>
auto push(F&& f, Rest&&... rest) -> std::future<decltype(f(0, rest...))>;

// Для функций без параметров
template<typename F>
auto push(F&& f) -> std::future<decltype(f(0))>;
```

#### Вспомогательные методы
```cpp
std::function<void(int)> pop();            // Извлечь задачу из очереди
std::thread& get_thread(int i);            // Получить ссылку на поток (с осторожностью!)
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
2. **Длительные задачи**: Избегайте очень длительных задач (разбивайте на подзадачи)
3. **Баланс нагрузки**: Следите за количеством бездействующих потоков `numIdle()`
4. **Память**: Большое количество задач может потреблять значительную память

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
