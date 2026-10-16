#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <queue>
#include <thread>
#include <unistd.h>

#include "pool.h"

using namespace std;

class my_pool : public thread_pool {
  // Hint: the reference solution uses an atomic variable, a queue, a mutex, a
  // condition variable, two function pointers, and a vector.  You probably
  // can't implement a pool with less.

  // Hint: you might want to add additional private methods to this class.  For
  // example, in the reference solution, one of the methods of this class is the
  // function that each thread runs.

public:
  /// construct a thread pool by providing a size and the function to run on
  /// each element that arrives in the queue
  ///
  /// @param size    The number of threads in the pool
  /// @param handler The code to run whenever something arrives in the pool
  my_pool(int size, function<bool(int)> handler) {
    cout << "Error: my_pool() constructor is not implemented\n";
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
  }

  /// Allow a user of the pool to see if the pool has been shut down
  virtual bool check_active() {
    cout << "my_pool::check_active() is not implemented\n";
    return false;
  }

  /// Shutting down the pool can take some time.  await_shutdown() lets a user
  /// of the pool wait until the threads are all done servicing clients.
  virtual void await_shutdown() {
    cout << "my_pool::await_shutdown() is not implemented";
  }

  /// When a new connection arrives at the server, it calls this to pass the
  /// connection to the pool for processing.
  ///
  /// @param sd The socket descriptor for the new connection
  virtual void service_connection(int sd) {
    cout << "my_pool::service_connection() is not implemented\n";
  }
};

/// Create a thread_pool object.
///
/// We use a factory pattern (with private constructor) to ensure that anyone
thread_pool *pool_factory(int size, function<bool(int)> handler) {
  // Hint: Don't change this function!
  return new my_pool(size, handler);
}