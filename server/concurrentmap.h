#include <functional>
#include <iostream>
#include <list>
#include <mutex>
#include <string>
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

public:
  /// Construct by specifying the number of buckets it should have
  ///
  /// @param _buckets The number of buckets
  ConcurrentMap(size_t _buckets) {
    std::cout << "ConcurrentMap::ConcurrentMap() is not implemented";
  }

  /// Destruct the ConcurrentMap
  virtual ~ConcurrentMap() {
    std::cout << "ConcurrentMap::~ConcurrentMap() is not implemented";
  }

  /// Clear the map.  This operation needs to use 2pl
  virtual void clear() {
    std::cout << "ConcurrentMap::clear() is not implemented";
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
    std::cout << "ConcurrentMap::insert() is not implemented";
    return false;
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
    std::cout << "ConcurrentMap::upsert() is not implemented";
    return false;
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
    std::cout << "ConcurrentMap::do_with() is not implemented";
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
    std::cout << "ConcurrentMap::do_with_readonly() is not implemented";
    return false;
  }

  /// Remove the mapping from a key to its value
  ///
  /// @param key        The key whose mapping should be removed
  ///
  /// @return true if the key was found and the value unmapped, false otherwise
  virtual bool remove(K key) {
    std::cout << "ConcurrentMap::remove() is not implemented";
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
    std::cout << "ConcurrentMap::do_all_readonly() is not implemented";
  }
};