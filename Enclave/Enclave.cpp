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

using namespace std;

#ifndef PATH_MAX
  #define PATH_MAX 1024
#endif /* PATH_MAX */

FileSystem::FileSystem(const size_t block_size) {
  this->block_size = block_size;
  this->files = new std::map<std::string, std::vector<std::vector<char>*>*>();
  this->directories = new std::map<std::string, bool>();
}

FileSystem::FileSystem(std::map<std::string, std::vector<std::vector<char>*>*>* restored_files) {
  this->block_size = FileSystem::DEFAULT_BLOCK_SIZE;
  this->files = restored_files;
  this->directories = new std::map<std::string, bool>();
  for (auto it = this->files->begin(); it != this->files->end(); it++) {
    std::string filename = it->first;
    std::vector<std::string>* tokens = split_path(filename);
    std::string directory_name;
    for (size_t i = 0; i < tokens->size() - 1; i++) {
      directory_name += tokens->at(i) + "/";
      this->mkdir(directory_name);
    }
    delete tokens;
  }
}

FileSystem::~FileSystem() {
  for (auto it = this->files->begin(); it != this->files->end(); it++) {
    auto blocks = it->second;
    for (auto block = blocks->begin(); block != blocks->end(); block++) {
      block = blocks->erase(block);
    }
    it = this->files->erase(it);
  }
  delete files;
  delete directories;
}

int FileSystem::create(const std::string &path) {
  std::string directory_path = get_directory(path);
  if (this->directories->find(directory_path) != this->directories->end()) {
    return -ENOTDIR;
  }
  if (this->directories->find(path) == this->directories->end()) {
    return -EISDIR;
  }
  if (this->files->find(path) != this->files->end()) {
    return -EEXIST;
  }
  (*this->files)[path] = new std::vector<std::vector<char>*>();
  return 0;
}

int FileSystem::unlink(const std::string &path) {
  std::string directory_path = get_directory(path);
  if (this->directories->find(directory_path) != this->directories->end()) {
    return -ENOTDIR;
  }
  if (this->directories->find(path) == this->directories->end()) {
    return -EISDIR;
  }
  if (this->files->find(path) == this->files->end()) {
    return -ENOENT;
  }
  this->files->erase(path);
  return 0;
}

int FileSystem::write(const std::string &path, char *data, const size_t offset, const size_t length) {
  std::string filename = clean_path(path);
  auto entry = this->files->find(filename);
  if (entry == this->files->end()) {
      return -ENOENT;
  }
  auto blocks = entry->second;
  size_t block_index = static_cast<size_t>(floor(static_cast<long double>(offset) / this->block_size));
  auto offset_in_block = offset % this->block_size;
  size_t written = 0;
  if (block_index < blocks->size()) {
      std::vector<char> *block = (*blocks)[block_index];
      size_t bytes_to_write = length;
      if (this->block_size < (offset_in_block + length)) {
        bytes_to_write = this->block_size - offset_in_block;
        block->resize(this->block_size);
      } else {
        block->resize(offset_in_block + length);
      }
      memcpy(block->data() + offset_in_block, data, bytes_to_write);
      written += bytes_to_write;
  }
  // Create new blocks from scratch
  while (written < length) {
    size_t bytes_to_write = length - written;
    if (this->block_size < bytes_to_write) {
      bytes_to_write = this->block_size;
    }
    std::vector<char> *block = new std::vector<char>(bytes_to_write);
    memcpy(block->data(), data + written, bytes_to_write);
    blocks->push_back(block);
    written += bytes_to_write;
  }
  return written;
}

int FileSystem::read_data(const std::vector<std::vector< char>*>* blocks,
                          char *buffer,
                          const size_t block_index,
                          const size_t offset,
                          const size_t size) {
  size_t read = 0;
  size_t offset_in_block = offset % this->block_size;
  for (size_t index = block_index;
       index < blocks->size() && read < size;
       index++, offset_in_block = 0) {
    std::vector<char> *block = blocks->at(index);
    auto size_to_copy = size - read;
    if (size_to_copy > block->size()) {
      size_to_copy = block->size();
    }
    memcpy(buffer + read, block->data() + offset_in_block, size_to_copy);
    read += size_to_copy;
  }
  return static_cast<int>(read);
}

int FileSystem::read(const std::string &path, char *data, const size_t offset, const size_t length) {
  std::string filename = clean_path(path);
  auto entry = this->files->find(filename);
  if (entry == this->files->end()) {
    return -ENOENT;
  }
  auto blocks = entry->second;
  size_t block_index = offset / this->block_size;
  if (blocks->size() <= block_index) {
    return 0;
  }
  int read = this->read_data(blocks, data, block_index, offset, length);
  return read;
}

int FileSystem::mkdir(const std::string &directory) {
  if (this->directories->find(directory) != this->directories->end()) {
    return -EISDIR;
  }
  if (this->files->find(directory) != this->files->end()) {
    return -ENOTDIR;
  }
  std::string parent_directory = get_directory(directory);
  if (this->directories->find(parent_directory) != this->directories->end()) {
    return -ENOTDIR;
  }
  (*this->directories)[directory] = true;
  return 0;
}

int FileSystem::rmdir(const std::string &directory) {
  if (this->directories->find(directory) == this->directories->end()) {
    return -ENOENT;
  }
  this->directories->erase(directory);
  return 0;
}

std::vector<std::string> FileSystem::readdir(const std::string &path) const {
  std::string pathname = clean_path(path);
  std::vector<std::string> entries;
  if (pathname.empty()) {
    return entries;
  }
  if (this->directories->find(pathname) != this->directories->end()) {
    return entries;
  }
  if (this->directories->find(pathname) == this->directories->end()) {
    return entries;
  }
  for (auto it = this->directories->begin(); it != this->directories->end(); it++) {
    if (is_in_directory(pathname, it->first)) {
      entries.push_back(get_relative_path(pathname, it->first));
    }
  }
  for (auto it = this->files->begin(); it != this->files->end(); it++) {
    if (is_in_directory(pathname, it->first)) {
      entries.push_back(get_relative_path(pathname, it->first));
    }
  }
  return entries;
}

size_t FileSystem::get_block_size() const {
  return this->block_size;
}

std::string FileSystem::strip_leading_slash(const std::string &filename) {
    std::string stripped = filename;
    while (stripped.length() > 0 && stripped.front() == '/') {
        stripped = stripped.substr(1, std::string::npos);
    }
    return stripped;
}

std::string FileSystem::strip_trailing_slash(const std::string &filename) {
    std::string stripped = filename;
    while (stripped.length() > 0 && stripped.back() == '/') {
        stripped.pop_back();
    }
    return stripped;
}

std::string FileSystem::clean_path(const std::string &filename) {
    std::string trimmed = strip_leading_slash(strip_trailing_slash(filename));
    size_t position;
    while ((position = trimmed.find("//")) != std::string::npos) {
        trimmed = trimmed.replace(position, 2, "/");
    }
    return trimmed;
}

bool FileSystem::starts_with(const std::string &pattern, const std::string &path) {
    if (path.length() <= pattern.length()) {
        return false;
    }
    return path.compare(0, pattern.length(), pattern.c_str()) == 0;
}

std::string FileSystem::get_relative_path(const std::string &directory, const std::string &file) {
    std::string directory_path = clean_path(directory);
    std::string file_path = clean_path(file);
    if (!starts_with(directory_path, file_path)) {
        throw std::runtime_error("directory and file do not start with the same substring");
    }
    return clean_path(file_path.substr(directory_path.length(), std::string::npos));
}

std::string FileSystem::get_directory(const std::string &path) {
  std::string absolute_path = clean_path(path);
  size_t pos = absolute_path.rfind("/");
  if (pos == std::string::npos) {
    return "/";
  }
  return absolute_path.substr(0, pos);
}

/**
 * Test if a file is located in a directory NOT in its subdirectories.
 * @param directory Path to the directory
 * @param file Path to the file
 * @return True if the file is directly located in the directory. False otherwise
 */
bool FileSystem::is_in_directory(const std::string &directory, const std::string &file) {
    std::string directory_path = clean_path(directory);
    std::string file_path = clean_path(file);
    if (!starts_with(directory_path, file_path)) {
        return false;
    }
    std::string relative_file_path = get_relative_path(directory_path, file_path);
    if (relative_file_path.find("/") != std::string::npos) {
        return false;
    }
    return true;
}

std::vector<std::string>* FileSystem::split_path(const std::string &path) {
  std::vector<std::string>* tokens = new std::vector<std::string>();
  std::string trimmed_path = clean_path(path);
  while (trimmed_path.length() > 0) {
    size_t start = 0, pos = 0;
    pos = trimmed_path.find("/", start);
    if (pos == std::string::npos) {
      tokens->push_back(trimmed_path.substr(start, trimmed_path.length()));
      break;
    }
    size_t length = pos - start;
    if (length > 0) {
      std::string token = trimmed_path.substr(start, length);
      if (token.length() > 0) {
        tokens->push_back(token);
      }
    }
    trimmed_path = clean_path(trimmed_path.substr(pos + 1, std::string::npos));
  }
  return tokens;
}


static map<string, vector<char>>* files;
static FileSystem* FILE_SYSTEM;

static string strip_leading_slash(string filename) {
  if (filename.size() > 0 && filename[0] == '/') {
    return filename.substr(1, string::npos);
  }
  return filename;
}

int ramfs_file_exists(const char* filename) {
  string name(filename);
  return files->find(filename) != files->end() ? 1 : 0;
}

int ramfs_get(const char* filename,
              long offset,
              size_t size,
              char* buffer) {
  vector<char> &file = (*files)[filename];

  size_t len = file.size();
  if (offset < len) {
    if (offset + size > len) {
      size = len - offset;
    }
    for (size_t i = 0; i < size; i++) {
      buffer[i] = file[offset + i];
    }
    return size;
  }
  return -EINVAL;
}

int ramfs_put(const char *filename,
              long offset,
              size_t size,
              const char *data) {
  string path = strip_leading_slash(filename);
  if (!ramfs_file_exists(path.c_str())) {
    return -ENOENT;
  }
  vector<char> *file = &((*files)[filename]);
  size_t i = 0;
  if (offset < file->size()) {
    auto room_for = file->size() - offset;
    for (; i < room_for; i++) {
      (*file)[offset + i] = data[i];
    }
  }
  for (; i < size; i++) {
     file->push_back(data[i]);
  }
  return size;
}

int ramfs_get_size(const char *pathname) {
  if (!ramfs_file_exists(pathname)) {
    return -1;
  }
  return (*files)[strip_leading_slash(pathname)].size();
}

int ramfs_trunkate(const char* path, size_t length) {
  if (!ramfs_file_exists(path)) {
    return -ENOENT;
  }
  string filename = strip_leading_slash(path);
  vector<char> &file = (*files)[filename];
  file.resize(length);
  return 0;
}

int ramfs_get_number_of_entries() {
  return files->size();
}

int ramfs_list_entries(char*entries, size_t length) {
  size_t i = 0;
  const size_t offset = 256;
  size_t number_of_entries = 0;
  for (auto it = files->begin(); it != files->end() && i < length; it++, i += offset, number_of_entries++) {
    string name = it->first;
    //memcpy seems to do the trick bug str::cpy fails miserably here
    memcpy(entries + i, name.c_str(), name.length());
    entries[i + name.length()] = '\0';
  }
  return number_of_entries;
}

int ramfs_create_file(const char *path) {
  string filename = strip_leading_slash(path);
  (*files)[filename] = vector<char>();
  return 0;
}

int ramfs_delete_file(const char *pathname) {
  files->erase(strip_leading_slash(pathname));
  return 0;
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
  sgx_status_t status = sgx_unseal_data(encrypted, NULL, NULL, (uint8_t*) plaintext, &data_size);
  return status;
}


int sgxfs_dump(const char* pathname,
               sgx_sealed_data_t* sealed_data,
               size_t sealed_size) {
  string path(pathname);
  auto entry = files->find(path);
  if (entry == files->end()) {
    return -ENOENT;
  }
  auto bytes = entry->second;
  size_t data_size = bytes.size();
  sealed_size = sizeof(sgx_sealed_data_t) + data_size;
  uint8_t* buffer = new uint8_t[data_size];
  // TODO(dburihabwa) Replace with a call to memcpy or pass bytes.data() c++11
  for (size_t i = 0; i < data_size; i++) {
    buffer[i] = bytes[i];
  }
  sgx_status_t ret = sgx_seal_data(0, NULL, data_size, buffer, sealed_size, sealed_data);
  delete [] buffer;
  return ret;
}

int sgxfs_restore(const char* pathname,
                  const sgx_sealed_data_t* sealed_data,
                  size_t sealed_size) {
  string path(pathname);
  auto entry = files->find(path);
  if (entry != files->end()) {
    return 0;
  }
  uint32_t data_size = sealed_data->aes_data.payload_size;
  uint8_t* plaintext = new uint8_t[data_size];
  sgx_status_t ret = sgx_unseal_data(sealed_data, NULL, NULL, plaintext, &data_size);
  vector<char> bytes(data_size);
  for (size_t i = 0; i < data_size; i++) {
    bytes[i] = plaintext[i];
  }
  (*files)[path] = bytes;
  return ret;
}

int init_filesystem() {
  files = new map<string, vector<char>>();
  FILE_SYSTEM = new FileSystem(FileSystem::DEFAULT_BLOCK_SIZE);
  return 0;
}

int destroy_filesystem() {
  if (files != NULL) {
    delete files;
  }
  if (FILE_SYSTEM != NULL) {
    delete FILE_SYSTEM;
  }
  return -1;
}
