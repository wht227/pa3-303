#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "../common/constants.h"
#include "../common/contextmanager.h"
#include "../common/err.h"

#include "authtableentry.h"
#include "map.h"
#include "map_factories.h"
#include "storage.h"

using namespace std;

/// MyStorage is the student implementation of the Storage class
class MyStorage : public Storage {
  /// The map of authentication information, indexed by username
  Map<string, AuthTableEntry> *auth_table;

  /// The map of key/value pairs
  Map<string, vector<uint8_t>> *kv_store;

  /// The name of the file from which the Storage object was loaded, and to
  /// which we persist the Storage object every time it changes
  const string filename;

public:
  MyStorage(const std::string &fname, size_t buckets)
      : auth_table(authtable_factory(buckets)),
        kv_store(kvstore_factory(buckets)), filename(fname) {
    cout << "my_storage.cc::MyStorage() is not implemented\n";
  }

  virtual ~MyStorage() {
    cout << "my_storage.cc::~MyStorage() is not implemented\n";
  }

  virtual result_t add_user(const string &user, const string &pass) {
    cout << "my_storage.cc::add_user() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  }

  virtual result_t set_user_data(const string &user, const string &pass,
                                 const vector<uint8_t> &content) {
    cout << "my_storage.cc::set_user_data() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  }

  virtual result_t get_user_data(const string &user, const string &pass,
                                 const string &who) {
    cout << "my_storage.cc::get_user_data() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  }

  virtual result_t get_all_users(const string &user, const string &pass) {
    cout << "my_storage.cc::get_all_users() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  }

  virtual result_t auth(const string &user, const string &pass) {
    cout << "my_storage.cc::auth() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  }

  virtual result_t kv_insert(const string &user, const string &pass,
                             const string &key, const vector<uint8_t> &val) {
    cout << "my_storage.cc::kv_insert() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  };

  virtual result_t kv_get(const string &user, const string &pass,
                          const string &key) {
    cout << "my_storage.cc::kv_get() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  };

  virtual result_t kv_delete(const string &user, const string &pass,
                             const string &key) {
    cout << "my_storage.cc::kv_delete() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  };

  virtual result_t kv_upsert(const string &user, const string &pass,
                             const string &key, const vector<uint8_t> &val) {
    cout << "my_storage.cc::kv_upsert() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  };

  virtual result_t kv_all(const string &user, const string &pass) {
    cout << "my_storage.cc::kv_all() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  };

  virtual void shutdown() {}

  virtual result_t save_file() {
    cout << "my_storage.cc::save_file() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  }

  virtual result_t load_file() {
    FILE *storage_file = fopen(filename.c_str(), "r");
    if (storage_file == nullptr) {
      return {true, "File not found: " + filename, {}};
    }

    // Hint: Don't change the previous 4 lines!

    cout << "my_storage.cc::load_file() is not implemented\n";
    return {false, RES_ERR_UNIMPLEMENTED, {}};
  }
};

Storage *storage_factory(const std::string &fname, size_t buckets) {
  return new MyStorage(fname, buckets);
}