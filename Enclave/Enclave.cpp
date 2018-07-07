#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <map>
#include <string>
#include <vector>

#include "sgx_trts.h"
#include "sgx_tseal.h"


#include "Enclave_t.h"
#include "../utils/filesystem.hpp"

static FileSystem* FILE_SYSTEM;

static std::string strip_leading_slash(std::string filename) {
  if (filename.size() > 0 && filename[0] == '/') {
    return filename.substr(1, std::string::npos);
  }
  return filename;
}

int enclave_is_file(const char* filename) {
  std::string cleaned_path = FileSystem::clean_path(filename);
  if (FILE_SYSTEM->is_file(cleaned_path)) {
    return EEXIST;
  }
  if (FILE_SYSTEM->is_directory(cleaned_path)) {
    return -EISDIR;
  }
  return -ENOENT;
}

int ramfs_get(const char* filename,
              int64_t offset,
              size_t size,
              char* buffer) {
  std::string cleaned_path = FileSystem::clean_path(filename);
  return FILE_SYSTEM->read(cleaned_path, buffer, offset, size);
}

int ramfs_put(const char *filename,
              int64_t offset,
              size_t size,
              const char *data) {
  std::string cleaned_path = FileSystem::clean_path(filename);
  return FILE_SYSTEM->write(cleaned_path, data, offset, size);
}

int ramfs_get_size(const char *pathname) {
  if (!FILE_SYSTEM->is_file(pathname)) {
    return -ENOENT;
  }
  return FILE_SYSTEM->get_file_size(pathname);
}

int ramfs_trunkate(const char* path, size_t length) {
  if (!FILE_SYSTEM->is_file(path)) {
    return -ENOENT;
  }
  return FILE_SYSTEM->truncate(path, length);
}

int ramfs_get_number_of_entries() {
  return FILE_SYSTEM->get_number_of_entries("/");
}

int enclave_readdir(const char* path, char* entries, size_t length) {
  std::string directory = FileSystem::clean_path(path);
  std::vector<std::string> files;
  try {
    files = FILE_SYSTEM->readdir(directory);
  } catch (const std::exception&) {
    return -ENOENT;
  }
  size_t i = 0;
  size_t number_of_entries = 0;
  for (auto it = files.begin();
       it != files.end() && i < length;
       it++, number_of_entries++) {
    std::string name = (*it);
    memcpy(entries + i, name.c_str(), name.length());
    entries[i + name.length()] = 0x1C;
    i += name.length() + 1;
  }
  return number_of_entries;
}

int ramfs_create_file(const char *path) {
  std::string pathname = FileSystem::clean_path(path);
  return FILE_SYSTEM->create(pathname);
}

int ramfs_delete_file(const char *pathname) {
  return FILE_SYSTEM->unlink(FileSystem::clean_path(pathname));
}

sgx_status_t ramfs_encrypt(const char* filename,
                  uint8_t* plaintext,
                  size_t size,
                  sgx_sealed_data_t* encrypted,
                  size_t sealed_size) {
  sgx_status_t status = sgx_seal_data(0, NULL, size, plaintext, sealed_size, encrypted);
  return status;
}

sgx_status_t ramfs_decrypt(const char* filename,
                  sgx_sealed_data_t* encrypted,
                  size_t sealed_size,
                  uint8_t* plaintext,
                  size_t size) {
  uint32_t data_size = size;
  sgx_status_t status = sgx_unseal_data(encrypted, NULL, NULL, reinterpret_cast<uint8_t*>(plaintext), &data_size);
  return status;
}


int sgxfs_dump(const char* pathname,
               sgx_sealed_data_t* sealed_data,
               size_t sealed_size) {
  std::string path = FileSystem::clean_path(pathname);
  if (!FILE_SYSTEM->is_file(path)) {
    return -ENOENT;
  }
  size_t data_size = FILE_SYSTEM->get_file_size(path);
  uint8_t* buffer = new uint8_t[data_size];
  FILE_SYSTEM->read(pathname, reinterpret_cast<char*>(buffer), 0, data_size);
  sealed_size = sizeof(sgx_sealed_data_t) + data_size;
  sgx_status_t ret = sgx_seal_data(0, NULL, data_size, buffer, sealed_size, sealed_data);
  delete [] buffer;
  return ret;
}

int sgxfs_restore(const char* pathname,
                  const sgx_sealed_data_t* sealed_data,
                  size_t sealed_size) {
  std::string path = FileSystem::clean_path(pathname);
  if (!FILE_SYSTEM->is_file(path)) {
    return 0;
  }
  FILE_SYSTEM->create(path);
  uint32_t data_size = sealed_data->aes_data.payload_size;
  uint8_t* plaintext = new uint8_t[data_size];
  sgx_status_t ret = sgx_unseal_data(sealed_data, NULL, NULL, plaintext, &data_size);
  FILE_SYSTEM->write(path, (const char*) plaintext, 0, data_size);
  return ret;
}

int enclave_mkdir(const char* pathname) {
  return FILE_SYSTEM->mkdir(std::string(pathname));
}

int init_filesystem() {
  FILE_SYSTEM = new FileSystem(FileSystem::DEFAULT_BLOCK_SIZE);
  return 0;
}

int destroy_filesystem() {
  if (FILE_SYSTEM != NULL) {
    delete FILE_SYSTEM;
  }
  return -1;
}
