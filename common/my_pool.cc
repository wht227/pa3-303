#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <unistd.h>
#include <vector>

#include "pool.h"

using namespace std;

class my_pool : public thread_pool {
  // Hint: the reference solution uses an atomic variable, a queue, a mutex, a
  // condition variable, two function pointers, and a vector.  You probably
  // can't implement a pool with less.

  // Hint: you might want to add additional private methods to this class.  For
  // example, in the reference solution, one of the methods of this class is the
  // function that each thread runs.

  /// Flag indicating whether a shutdown signal has been received
  atomic<bool> received_shutdown;

  /// Queue of client socket descriptors waiting to be processed
  queue<int> task_queue;

  /// Mutex protecting access to task_queue
  mutex task_queue_lock;

  /// Condition variable to put worker threads to sleep when the queue is empty
  condition_variable cv;

  /// Function to process a single client connection socket
  function<bool(int)> service_routine;

  /// Function to notify the server when a thread receives a shutdown request
  function<void()> shutdown_handler;

  /// Vector storing the worker threads in the pool
  vector<thread> threads;

  /// Main worker loop executed by every thread in the pool
  void thread_task() {
    // Loop until a shutdown signal is received
    while (received_shutdown == false) {
      // Acquire lock to safely access the task queue
      unique_lock<mutex> lock(task_queue_lock);

      // If the queue is empty, wait for a new task to be added
      if (task_queue.empty() && received_shutdown == false) {
        cv.wait(lock);
      }

      // Dequeue a task if one is available
      int client_socket = -1;
      if (!task_queue.empty()) {
        client_socket = task_queue.front();
        task_queue.pop();
      }

      // Release the lock so other threads can access the queue
      lock.unlock();

      // Process the request if a valid socket was dequeued
      if (client_socket > -1) {
        bool should_shutdown = service_routine(client_socket);
        close(client_socket);

        // Check if the service routine signaled a server shutdown
        if (should_shutdown == true) {
          received_shutdown = true;

          // Re-acquire lock to safely notify other threads
          lock.lock();
          cv.notify_all(); // Wake up all waiting threads
          lock.unlock();

          // Call the final shutdown handler
          if (shutdown_handler) {
            shutdown_handler();
          }
        }
      }
    }
  }

public:
  /// construct a thread pool by providing a size and the function to run on
  /// each element that arrives in the queue
  ///
  /// @param size    The number of threads in the pool
  /// @param handler The code to run whenever something arrives in the pool
  my_pool(int size, function<bool(int)> handler) {
    received_shutdown = false;
    service_routine = handler;

    // Create worker threads and have each run thread_task()
    for (int i = 0; i < size; i++) {
      threads.push_back(thread(&my_pool::thread_task, this));
    }
  }

  /// destruct a thread pool
  ///
  /// Hint: If you do things right, you probably won't need to write a
  /// destructor
  virtual ~my_pool() = default;

  /// Allow a user of the pool to provide some code to run when the pool decides
  /// it needs to shut down.
  ///
  /// @param func The code that should be run when the pool shuts down
  virtual void set_shutdown_handler(function<void()> func) {
    // Hint: You probably just need to save the function for later.
    shutdown_handler = func;
  }

  /// Allow a user of the pool to see if the pool has been shut down
  virtual bool check_active() {
    return (received_shutdown == false);
  }

  /// Shutting down the pool can take some time.  await_shutdown() lets a user
  /// of the pool wait until the threads are all done servicing clients.
  virtual void await_shutdown() {
    // Wait for every worker thread in the pool to finish
    for (size_t i = 0; i < threads.size(); i++) {
      if (threads[i].joinable()) {
        threads[i].join();
      }
    }
  }

  /// When a new connection arrives at the server, it calls this to pass the
  /// connection to the pool for processing.
  ///
  /// @param sd The socket descriptor for the new connection
  virtual void service_connection(int sd) {
    // Lock the queue, push the new client socket, and wake up one worker thread
    unique_lock<mutex> lock(task_queue_lock);
    task_queue.push(sd);
    cv.notify_one();
    lock.unlock();
  }
};

/// Create a thread_pool object.
///
/// We use a factory pattern (with private constructor) to ensure that anyone
thread_pool *pool_factory(int size, function<bool(int)> handler) {
  // Hint: Don't change this function!
  return new my_pool(size, handler);
}
