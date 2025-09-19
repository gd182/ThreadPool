# ThreadPool

A thread pool library with support for asynchronous task execution, dynamic management, and thread safety.

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [API Documentation](#api-documentation)
- [Usage Examples](#usage-examples)
- [Exception Handling](#exception-handling)
- [Performance](#performance)
- [License](#license)

## Features

- **Thread Safety** - Full mutex protection
- **Dynamic Management** - Resize pool during runtime
- **Asynchronous Tasks** - `std::future` support for result retrieval
- **Exception Handling** - Task exceptions don't crash the pool
- **Monitoring** - Track number of idle threads
- **Memory Management** - Automatic resource cleanup
- **Flexibility** - Support for functions with and without parameters

## Quick Start

### Basic Example

```cpp
#include "ThreadPool.h"
#include <iostream>

void simple_task(int thread_id) {
    std::cout << "Thread " << thread_id << " is working!" << std::endl;
}

int main() {
    // Create pool with 4 threads
    tp::ThreadPool pool(4);
    
    // Add tasks
    for (int i = 0; i < 10; ++i) {
        pool.push(simple_task);
    }
    
    // Graceful shutdown
    pool.stop(true);
    return 0;
}
```

### Example with Return Values

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
    
    // Get results
    for (size_t i = 0; i < futures.size(); ++i) {
        double result = futures[i].get();
        std::cout << "2^" << i << " = " << result << std::endl;
    }
    
    pool.stop(true);
    return 0;
}
```

## API Documentation

### Core Methods

#### Constructors
```cpp
tp::ThreadPool();                          // Default thread count pool
tp::ThreadPool(int countThreads);          // Pool with specified thread count
```

#### Pool Management
```cpp
void resize(int countThreads);             // Resize the pool
void stop(bool isWait = false);            // Stop the pool (true for graceful)
void clearQueue();                         // Clear task queue
int size();                                // Get current pool size
int numIdle();                             // Get number of idle threads
```

#### Task Submission
```cpp
// For functions with parameters
template<typename F, typename... Rest>
auto push(F&& f, Rest&&... rest) -> std::future<decltype(f(0, rest...))>;

// For functions without parameters
template<typename F>
auto push(F&& f) -> std::future<decltype(f(0))>;
```

#### Utility Methods
```cpp
std::function<void(int)> pop();            // Pop task from queue
std::thread& get_thread(int i);            // Get thread reference (use with caution!)
```

## Usage Examples

### Parallel Data Processing

```cpp
#include "ThreadPool.h"
#include <vector>
#include <algorithm>

void process_chunk(int thread_id, std::vector<int>& data, int start, int end) {
    for (int i = start; i < end; ++i) {
        data[i] = data[i] * 2 + 1; // Some processing
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
    
    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.get();
    }
    
    pool.stop(true);
    return 0;
}
```

### Dynamic Pool Management

```cpp
#include "ThreadPool.h"
#include <chrono>

int main() {
    tp::ThreadPool pool(2);
    
    // Start with small pool
    for (int i = 0; i < 10; ++i) {
        pool.push([](int id) { 
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
        });
    }
    
    // Increase pool size for heavier load
    pool.resize(8);
    
    // Add more tasks
    for (int i = 0; i < 20; ++i) {
        pool.push([](int id) { 
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
        });
    }
    
    // Reduce pool size after peak load
    pool.resize(4);
    
    pool.stop(true);
    return 0;
}
```

### Using in Your Project

1. Copy these files to your project:
   - `QueueMutex.h`
   - `ThreadPool.h`
   - `ThreadPool.cpp`

2. Add to your CMakeLists.txt:
```cmake
add_executable(your_project main.cpp ThreadPool.cpp)
target_compile_features(your_project PRIVATE cxx_std_11)
target_link_libraries(your_project PRIVATE pthread)
```

## Exception Handling

The library properly handles exceptions in tasks:

```cpp
try {
    auto future = pool.push([](int id) {
        throw std::runtime_error("Something went wrong!");
    });
    future.get(); // Exception will be rethrown here
} catch (const std::exception& e) {
    std::cerr << "Caught exception: " << e.what() << std::endl;
}
```

## Performance

### Usage Recommendations

1. **Pool Size**: Use `std::thread::hardware_concurrency()` for optimal size
2. **Long Tasks**: Avoid very long-running tasks (break them into subtasks)
3. **Load Balancing**: Monitor idle thread count with `numIdle()`
4. **Memory**: Large number of tasks may consume significant memory

### Benchmark Example

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
            // Lightweight task
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

## License

This library is distributed under the MIT License. You can freely use it in both commercial and non-commercial projects.

## Support

If you have questions or suggestions for improvement, please create an issue in the project repository.

---

**Note**: This library is intended for educational  purposes. Always test in your environment before using in production.
