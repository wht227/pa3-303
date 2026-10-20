#include <functional>
#include <iostream>
#include <list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "map.h"

/// ConcurrentMap is a concurrent implementation of the Map interface (a
/// Key/Value store).  It is implemented as a vector of buckets, with one lock
/// per bucket.  Since the number of buckets is fixed, performance can suffer if
/// the thread count is high relative to the number of buckets.  Furthermore,
/// the asymptotic guarantees of this data structure are dependent on the
/// quality of the bucket implementation.  If a vector is used within the bucket
/// to store key/value pairs, then the guarantees will be poor if the key range
/// is large relative to the number of buckets.  If an unordered_map is used,
/// then the asymptotic guarantees should be strong.
///
/// The ConcurrentMap is templated on the Key and Value types.
///
/// This map uses std::hash to map keys to positions in the vector.  A
/// production map should use something better.
///
/// This map provides strong consistency guarantees: every operation uses
/// two-phase locking (2PL), and the lambda parameters to methods enable nesting
/// of 2PL operations across maps.
///
/// @tparam K The type of the keys in this map
/// @tparam V The type of the values in this map
template <typename K, typename V> class ConcurrentMap : public Map<K, V> {
  // Hint: The reference solution uses a vector of structs, where the struct
  // combines a mutex with a collection.  It also has a field for the number of
  // buckets.  You should think carefully about whether your vector should hold
  // those structs directly, or hold pointers to those structs.  Your decision
  // will determine if you need a real destructor or not.

  /// Each bucket holds its own mutex and a linked list of (key, value) pairs
  struct Bucket {
    std::mutex lock;
    std::list<std::pair<K, V>> entries;
  };

  /// Total number of buckets in the hash table
  size_t num_buckets;

  /// Vector of pointers to buckets (since std::mutex cannot be copied/moved)
  std::vector<Bucket *> buckets;

  /// Helper function to hash a key and find its bucket index
  size_t get_bucket_index(K key) {
    std::hash<K> hasher;
    return hasher(key) % num_buckets;
  }

public:
  /// Construct by specifying the number of buckets it should have
  ///
  /// @param _buckets The number of buckets
  ConcurrentMap(size_t _buckets) {
    num_buckets = _buckets;
    for (size_t i = 0; i < num_buckets; i++) {
      buckets.push_back(new Bucket());
    }
  }

  /// Destruct the ConcurrentMap
  virtual ~ConcurrentMap() {
    for (size_t i = 0; i < num_buckets; i++) {
      delete buckets[i];
    }
  }

  /// Clear the map.  This operation needs to use 2pl
  virtual void clear() {
    // Phase 1 (Growing phase): Acquire all bucket locks in order
    for (size_t i = 0; i < num_buckets; i++) {
      buckets[i]->lock.lock();
    }

    // Clear every bucket's linked list while all locks are held
    for (size_t i = 0; i < num_buckets; i++) {
      buckets[i]->entries.clear();
    }

    // Phase 2 (Shrinking phase): Release all bucket locks
    for (size_t i = 0; i < num_buckets; i++) {
      buckets[i]->lock.unlock();
    }
  }

  /// Insert the provided key/value pair only if there is no mapping for the key
  /// yet.
  ///
  /// @param key        The key to insert
  /// @param val        The value to insert
  ///
  /// @return true if the key/value was inserted, false if the key already
  ///         existed in the table
  virtual bool insert(K key, V val) {
    size_t idx = get_bucket_index(key);
    std::lock_guard<std::mutex> guard(buckets[idx]->lock);

    // Check if the key already exists in this bucket's list
    for (auto &pair : buckets[idx]->entries) {
      if (pair.first == key) {
        return false;
      }
    }

    // Key does not exist yet, so add it to the end of the bucket's list
    buckets[idx]->entries.push_back(std::make_pair(key, val));
    return true;
  }

  /// Insert the provided key/value pair if there is no mapping for the key yet.
  /// If there is a key, then update the mapping by replacing the old value with
  /// the provided value
  ///
  /// @param key    The key to upsert
  /// @param val    The value to upsert
  ///
  /// @return true if the key/value was inserted, false if the key already
  ///         existed in the table and was thus updated instead
  virtual bool upsert(K key, V val) {
    size_t idx = get_bucket_index(key);
    std::lock_guard<std::mutex> guard(buckets[idx]->lock);

    // Search for the key; if found, update its value and return false
    for (auto &pair : buckets[idx]->entries) {
      if (pair.first == key) {
        pair.second = val;
        return false;
      }
    }

    // Key was not found, so insert a new pair and return true
    buckets[idx]->entries.push_back(std::make_pair(key, val));
    return true;
  }

  /// Apply a function to the value associated with a given key.  The function
  /// is allowed to modify the value.
  ///
  /// @param key The key whose value will be modified
  /// @param f   The function to apply to the key's value
  ///
  /// @return true if the key existed and the function was applied, false
  ///         otherwise
  virtual bool do_with(K key, std::function<void(V &)> f) {
    size_t idx = get_bucket_index(key);
    std::lock_guard<std::mutex> guard(buckets[idx]->lock);

    // Search for the key; if found, apply function f to its modifiable value
    for (auto &pair : buckets[idx]->entries) {
      if (pair.first == key) {
        f(pair.second);
        return true;
      }
    }

    return false;
  }

  /// Apply a function to the value associated with a given key.  The function
  /// is not allowed to modify the value.
  ///
  /// @param key The key whose value will be modified
  /// @param f   The function to apply to the key's value
  ///
  /// @return true if the key existed and the function was applied, false
  ///         otherwise
  virtual bool do_with_readonly(K key, std::function<void(const V &)> f) {
    size_t idx = get_bucket_index(key);
    std::lock_guard<std::mutex> guard(buckets[idx]->lock);

    // Search for the key; if found, apply function f to its const value
    for (const auto &pair : buckets[idx]->entries) {
      if (pair.first == key) {
        f(pair.second);
        return true;
      }
    }

    return false;
  }

  /// Remove the mapping from a key to its value
  ///
  /// @param key        The key whose mapping should be removed
  ///
  /// @return true if the key was found and the value unmapped, false otherwise
  virtual bool remove(K key) {
    size_t idx = get_bucket_index(key);
    std::lock_guard<std::mutex> guard(buckets[idx]->lock);

    // Iterate through the bucket's list and erase the matching element
    for (auto it = buckets[idx]->entries.begin();
         it != buckets[idx]->entries.end(); ++it) {
      if (it->first == key) {
        buckets[idx]->entries.erase(it);
        return true;
      }
    }

    return false;
  }

  /// Apply a function to every key/value pair in the map.  Note that the
  /// function is not allowed to modify keys or values.
  ///
  /// @param f    The function to apply to each key/value pair
  /// @param then A function to run when this is done, but before unlocking...
  ///             useful for 2pl
  virtual void do_all_readonly(std::function<void(const K, const V &)> f,
                               std::function<void()> then) {
    // Phase 1 (Growing phase): Lock all buckets in order
    for (size_t i = 0; i < num_buckets; i++) {
      buckets[i]->lock.lock();
    }

    // Apply function f to every (key, value) pair in every bucket
    for (size_t i = 0; i < num_buckets; i++) {
      for (const auto &pair : buckets[i]->entries) {
        f(pair.first, pair.second);
      }
    }

    // Run the 'then' function before releasing any locks (for 2PL chaining)
    if (then) {
      then();
    }

    // Phase 2 (Shrinking phase): Unlock all buckets
    for (size_t i = 0; i < num_buckets; i++) {
      buckets[i]->lock.unlock();
    }
  }
};
