#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <openssl/evp.h>
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

/// Helper function: append a salt to a password, hash it using SHA-256, and
/// return the resulting 32-byte digest vector.
///
/// @param pass The plain-text password to hash
/// @param salt The 16-byte salt to combine with the password
///
/// @return The hashed and salted password
vector<uint8_t> hash_pass(const string &pass, const vector<uint8_t> &salt) {
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    return {};
  }
  ContextManager cleanup([&]() { EVP_MD_CTX_free(ctx); });

  const EVP_MD *md = EVP_sha256();
  if (EVP_DigestInit_ex(ctx, md, nullptr) <= 0) {
    return {};
  }

  // Hash the password bytes followed by the salt bytes
  if (EVP_DigestUpdate(ctx, pass.c_str(), pass.length()) <= 0) {
    return {};
  }
  if (EVP_DigestUpdate(ctx, salt.data(), salt.size()) <= 0) {
    return {};
  }

  vector<uint8_t> hash(LEN_PASSHASH);
  unsigned int len = 0;
  if (EVP_DigestFinal_ex(ctx, hash.data(), &len) <= 0) {
    return {};
  }
  hash.resize(len);
  return hash;
}

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
  /// Construct an empty object and specify the file from which it should be
  /// loaded.  To avoid exceptions and errors in the constructor, the act of
  /// loading data is separate from construction.
  ///
  /// @param fname   The name of the file to use for persistence
  /// @param buckets The number of buckets in the hash table
  MyStorage(const std::string &fname, size_t buckets)
      : auth_table(authtable_factory(buckets)),
        kv_store(kvstore_factory(buckets)), filename(fname) {}

  /// Destructor for the storage object.
  virtual ~MyStorage() {
    delete auth_table;
    delete kv_store;
  }

  /// Create a new entry in the Auth table.  If the user already exists, return
  /// an error.  Otherwise, create a salt, hash the password, and then save an
  /// entry with the username, salt, hashed password, and a zero-byte content.
  ///
  /// @param user The user name to register
  /// @param pass The password to associate with that user name
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t add_user(const string &user, const string &pass) {
    // Generate 16 random bytes for the user's salt
    vector<uint8_t> salt(LEN_SALT);
    if (RAND_bytes(salt.data(), LEN_SALT) != 1) {
      return {false, RES_ERR_SERVER, {}};
    }

    // Hash the password + salt
    vector<uint8_t> pass_hash = hash_pass(pass, salt);

    // Create the new user entry with empty profile content
    AuthTableEntry entry;
    entry.username = user;
    entry.salt = salt;
    entry.pass_hash = pass_hash;
    entry.content = {};

    // Insert into auth_table; if key already exists, return error
    if (!auth_table->insert(user, entry)) {
      return {false, RES_ERR_USER_EXISTS, {}};
    }

    return {true, RES_OK, {}};
  }

  /// Set the data bytes for a user, but do so if and only if the password
  /// matches
  ///
  /// @param user    The name of the user whose content is being set
  /// @param pass    The password for the user, used to authenticate
  /// @param content The data to set for this user
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t set_user_data(const string &user, const string &pass,
                                 const vector<uint8_t> &content) {
    // Authenticate the user first
    result_t r = auth(user, pass);
    if (!r.succeeded) {
      return {false, r.msg, {}};
    }

    // Update the user's profile content
    bool updated = auth_table->do_with(user, [&](AuthTableEntry &entry) {
      entry.content = content;
    });
    if (!updated) {
      return {false, RES_ERR_SERVER, {}};
    }

    return {true, RES_OK, {}};
  }

  /// Return a copy of the user data for a user, but do so only if the password
  /// matches
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param who  The name of the user whose content is being fetched
  ///
  /// @return A result tuple, as described in storage.h.  Note that "no data" is
  ///         an error
  virtual result_t get_user_data(const string &user, const string &pass,
                                 const string &who) {
    // Authenticate the requesting user
    result_t r = auth(user, pass);
    if (!r.succeeded) {
      return {false, r.msg, {}};
    }

    // Look up the target user's profile data
    vector<uint8_t> data;
    bool found = auth_table->do_with_readonly(who, [&](const AuthTableEntry &entry) {
      data = entry.content;
    });

    if (!found) {
      return {false, RES_ERR_NO_USER, {}};
    }
    if (data.empty()) {
      return {false, RES_ERR_NO_DATA, {}};
    }

    return {true, RES_OK, data};
  }

  /// Return a newline-delimited string containing all of the usernames in the
  /// auth table
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t get_all_users(const string &user, const string &pass) {
    // Authenticate the requesting user
    result_t r = auth(user, pass);
    if (!r.succeeded) {
      return {false, r.msg, {}};
    }

    // Collect all usernames separated by newlines
    string user_list = "";
    auth_table->do_all_readonly(
        [&](const string &uname, const AuthTableEntry &) {
          if (!user_list.empty()) {
            user_list += "\n";
          }
          user_list += uname;
        },
        []() {});

    vector<uint8_t> data(user_list.begin(), user_list.end());
    return {true, RES_OK, data};
  }

  /// Authenticate a user
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t auth(const string &user, const string &pass) {
    bool pass_matched = false;
    bool user_found = auth_table->do_with_readonly(
        user, [&](const AuthTableEntry &entry) {
          vector<uint8_t> candidate_hash = hash_pass(pass, entry.salt);
          if (candidate_hash == entry.pass_hash) {
            pass_matched = true;
          }
        });

    if (!user_found || !pass_matched) {
      return {false, RES_ERR_LOGIN, {}};
    }

    return {true, RES_OK, {}};
  }

  /// Create a new key/value mapping in the table
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param key  The key whose mapping is being created
  /// @param val  The value to copy into the map
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t kv_insert(const string &user, const string &pass,
                             const string &key, const vector<uint8_t> &val) {
    auto r = auth(user, pass);
    if (!r.succeeded)
      return {false, r.msg, {}};
    if (!kv_store->insert(key, vector<uint8_t>(val)))
      return {false, RES_ERR_KEY, {}};
    return {true, RES_OK, {}};
  };

  /// Get a copy of the value to which a key is mapped
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param key  The key whose value is being fetched
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t kv_get(const string &user, const string &pass,
                          const string &key) {
    auto r = auth(user, pass);
    if (!r.succeeded)
      return {false, r.msg, {}};

    vector<uint8_t> val_copy;
    bool found = kv_store->do_with_readonly(
        key, [&](const vector<uint8_t> &val) { val_copy = val; });
    if (!found)
      return {false, RES_ERR_KEY, {}};
    return {true, RES_OK, val_copy};
  };

  /// Delete a key/value mapping
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param key  The key whose value is being deleted
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t kv_delete(const string &user, const string &pass,
                             const string &key) {
    auto r = auth(user, pass);
    if (!r.succeeded)
      return {false, r.msg, {}};
    if (!kv_store->remove(key))
      return {false, RES_ERR_KEY, {}};
    return {true, RES_OK, {}};
  };

  /// Insert or update, so that the given key is mapped to the give value
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param key  The key whose mapping is being upserted
  /// @param val  The value to copy into the map
  ///
  /// @return A result tuple, as described in storage.h.  Note that there are
  /// two
  ///         "OK" messages, depending on whether we get an insert or an update.
  virtual result_t kv_upsert(const string &user, const string &pass,
                             const string &key, const vector<uint8_t> &val) {
    auto r = auth(user, pass);
    if (!r.succeeded)
      return {false, r.msg, {}};
    if (kv_store->upsert(key, vector<uint8_t>(val)))
      return {true, RES_OKINS, {}};
    return {true, RES_OKUPD, {}};
  };

  /// Return all of the keys in the kv_store, as a "\n"-delimited string
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t kv_all(const string &user, const string &pass) {
    auto r = auth(user, pass);
    if (!r.succeeded)
      return {false, r.msg, {}};

    string key_list = "";
    kv_store->do_all_readonly(
        [&](const string &k, const vector<uint8_t> &) {
          if (!key_list.empty()) {
            key_list += "\n";
          }
          key_list += k;
        },
        []() {});

    if (key_list.empty())
      return {false, RES_ERR_NO_DATA, {}};

    vector<uint8_t> data(key_list.begin(), key_list.end());
    return {true, RES_OK, data};
  };

  /// Shut down the storage when the server stops.  This method needs to close
  /// any open files related to incremental persistence.  It also needs to clean
  /// up any state related to .so files.  This is only called when all threads
  /// have stopped accessing the Storage object.
  virtual void shutdown() {
    // Hint: You don't need anything for shutdown() in this assignment
  }

  /// Write the entire Storage object to the file specified by this.filename. To
  /// ensure durability, Storage must be persisted in two steps.  First, it must
  /// be written to a temporary file (this.filename.tmp).  Then the temporary
  /// file can be renamed to replace the older version of the Storage object.
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t save_file() {
    string temp_name = filename + ".tmp";
    FILE *f = fopen(temp_name.c_str(), "wb");
    if (f == nullptr) {
      return {false, RES_ERR_SERVER, {}};
    }

    bool error_occurred = false;

    // Chain do_all_readonly on auth_table and kv_store for strict 2PL
    auth_table->do_all_readonly(
        [&](const string &, const AuthTableEntry &entry) {
          if (error_occurred) {
            return;
          }

          // 1. Write 4-byte header "AUTH"
          if (fwrite(AUTHENTRY.c_str(), 1, 4, f) != 4) {
            error_occurred = true;
            return;
          }

          // 2. Write 4 lengths: username, salt, hashed password, profile
          uint32_t u_len = entry.username.length();
          uint32_t salt_len = entry.salt.size();
          uint32_t pass_len = entry.pass_hash.size();
          uint32_t content_len = entry.content.size();

          if (fwrite(&u_len, sizeof(uint32_t), 1, f) != 1 ||
              fwrite(&salt_len, sizeof(uint32_t), 1, f) != 1 ||
              fwrite(&pass_len, sizeof(uint32_t), 1, f) != 1 ||
              fwrite(&content_len, sizeof(uint32_t), 1, f) != 1) {
            error_occurred = true;
            return;
          }

          // 3. Write the actual bytes
          if (fwrite(entry.username.c_str(), 1, u_len, f) != u_len ||
              fwrite(entry.salt.data(), 1, salt_len, f) != salt_len ||
              fwrite(entry.pass_hash.data(), 1, pass_len, f) != pass_len) {
            error_occurred = true;
            return;
          }
          if (content_len > 0) {
            if (fwrite(entry.content.data(), 1, content_len, f) !=
                content_len) {
              error_occurred = true;
              return;
            }
          }

          // 4. Write padding (0 to 3 zero bytes so total entry is multiple of 4)
          size_t total_len = u_len + salt_len + pass_len + content_len;
          size_t pad = (4 - (total_len % 4)) % 4;
          if (pad > 0) {
            char zeros[4] = {0, 0, 0, 0};
            if (fwrite(zeros, 1, pad, f) != pad) {
              error_occurred = true;
              return;
            }
          }
        },
        [&]() {
          // While all auth_table locks are still held, lock kv_store and write
          // all KV pairs
          kv_store->do_all_readonly(
              [&](const string &key, const vector<uint8_t> &val) {
                if (error_occurred) {
                  return;
                }

                // 1. Write 4-byte header "KVKV"
                if (fwrite(KVENTRY.c_str(), 1, 4, f) != 4) {
                  error_occurred = true;
                  return;
                }

                // 2. Write 2 lengths: key length and value length
                uint32_t k_len = key.length();
                uint32_t v_len = val.size();
                if (fwrite(&k_len, sizeof(uint32_t), 1, f) != 1 ||
                    fwrite(&v_len, sizeof(uint32_t), 1, f) != 1) {
                  error_occurred = true;
                  return;
                }

                // 3. Write key bytes and value bytes
                if (fwrite(key.c_str(), 1, k_len, f) != k_len) {
                  error_occurred = true;
                  return;
                }
                if (v_len > 0) {
                  if (fwrite(val.data(), 1, v_len, f) != v_len) {
                    error_occurred = true;
                    return;
                  }
                }

                // 4. Write padding (0 to 3 zero bytes)
                size_t total_len = k_len + v_len;
                size_t pad = (4 - (total_len % 4)) % 4;
                if (pad > 0) {
                  char zeros[4] = {0, 0, 0, 0};
                  if (fwrite(zeros, 1, pad, f) != pad) {
                    error_occurred = true;
                    return;
                  }
                }
              },
              [&]() {
                // Close and atomically rename the file while all locks of both
                // tables are held
                fclose(f);
                if (!error_occurred) {
                  if (rename(temp_name.c_str(), filename.c_str()) != 0) {
                    error_occurred = true;
                  }
                }
              });
        });

    if (error_occurred) {
      return {false, RES_ERR_SERVER, {}};
    }

    return {true, RES_OK, {}};
  }

  /// Populate the Storage object by loading this.filename.  Note that load()
  /// begins by clearing the maps, so that when the call is complete, exactly
  /// and only the contents of the file are in the Storage object.
  ///
  /// @return A result tuple, as described in storage.h.  Note that a
  ///         non-existent file is not an error.
  virtual result_t load_file() {
    FILE *storage_file = fopen(filename.c_str(), "r");
    if (storage_file == nullptr) {
      return {true, "File not found: " + filename, {}};
    }

    // Hint: Don't change the previous 4 lines!

    // Clear both maps before populating them from the file
    auth_table->clear();
    kv_store->clear();

    while (true) {
      // Read 4-byte entry header ("AUTH" or "KVKV")
      char header_buf[4];
      size_t bytes_read = fread(header_buf, 1, 4, storage_file);
      if (bytes_read == 0) {
        // Reached end of file cleanly
        break;
      }
      if (bytes_read < 4) {
        fclose(storage_file);
        return {false, RES_ERR_SERVER, {}};
      }

      string header(header_buf, 4);

      if (header == AUTHENTRY) {
        // Read the 4 lengths for an AUTH entry
        uint32_t u_len = 0, salt_len = 0, pass_len = 0, content_len = 0;
        if (fread(&u_len, sizeof(uint32_t), 1, storage_file) != 1 ||
            fread(&salt_len, sizeof(uint32_t), 1, storage_file) != 1 ||
            fread(&pass_len, sizeof(uint32_t), 1, storage_file) != 1 ||
            fread(&content_len, sizeof(uint32_t), 1, storage_file) != 1) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }

        // Read username, salt, password hash, and profile content
        string u(u_len, '\0');
        if (u_len > 0 && fread(&u[0], 1, u_len, storage_file) != u_len) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }

        vector<uint8_t> salt(salt_len);
        if (salt_len > 0 &&
            fread(salt.data(), 1, salt_len, storage_file) != salt_len) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }

        vector<uint8_t> pass_hash(pass_len);
        if (pass_len > 0 &&
            fread(pass_hash.data(), 1, pass_len, storage_file) != pass_len) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }

        vector<uint8_t> content(content_len);
        if (content_len > 0 &&
            fread(content.data(), 1, content_len, storage_file) !=
                content_len) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }

        // Read and discard 0-3 padding bytes
        size_t total_len = u_len + salt_len + pass_len + content_len;
        size_t pad = (4 - (total_len % 4)) % 4;
        if (pad > 0) {
          char pad_bytes[4];
          if (fread(pad_bytes, 1, pad, storage_file) != pad) {
            fclose(storage_file);
            return {false, RES_ERR_SERVER, {}};
          }
        }

        AuthTableEntry entry{u, salt, pass_hash, content};
        if (!auth_table->insert(u, entry)) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }
      } else if (header == KVENTRY) {
        // Read the 2 lengths for a KVKV entry
        uint32_t k_len = 0, v_len = 0;
        if (fread(&k_len, sizeof(uint32_t), 1, storage_file) != 1 ||
            fread(&v_len, sizeof(uint32_t), 1, storage_file) != 1) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }

        // Read key and value
        string key(k_len, '\0');
        if (k_len > 0 && fread(&key[0], 1, k_len, storage_file) != k_len) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }

        vector<uint8_t> val(v_len);
        if (v_len > 0 && fread(val.data(), 1, v_len, storage_file) != v_len) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }

        // Read and discard 0-3 padding bytes
        size_t total_len = k_len + v_len;
        size_t pad = (4 - (total_len % 4)) % 4;
        if (pad > 0) {
          char pad_bytes[4];
          if (fread(pad_bytes, 1, pad, storage_file) != pad) {
            fclose(storage_file);
            return {false, RES_ERR_SERVER, {}};
          }
        }

        if (!kv_store->insert(key, val)) {
          fclose(storage_file);
          return {false, RES_ERR_SERVER, {}};
        }
      } else {
        // Unknown header
        fclose(storage_file);
        return {false, RES_ERR_SERVER, {}};
      }
    }

    fclose(storage_file);
    return {true, "Loaded: " + filename, {}};
  }
};

/// Create an empty Storage object and specify the file from which it should be
/// loaded.  To avoid exceptions and errors in the constructor, the act of
/// loading data is separate from construction.
///
/// @param fname   The name of the file to use for persistence
/// @param buckets The number of buckets in the hash table
Storage *storage_factory(const std::string &fname, size_t buckets) {
  return new MyStorage(fname, buckets);
}
