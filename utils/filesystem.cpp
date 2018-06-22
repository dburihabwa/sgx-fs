#include "filesystem.hpp"

#include <cmath>
#include <cstring>

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "../utils/fs.hpp"
#include "../utils/serialization.hpp"

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
    string filename = it->first;
    vector<string>* tokens = split_path(filename);
    string directory_name;
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
  (*this->files)[path] = new vector<vector<char>*>();
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
  auto start = chrono::high_resolution_clock::now();
  auto entry = this->files->find(filename);
  if (entry == this->files->end()) {
      return -ENOENT;
  }
  auto blocks = entry->second;
  auto block_index = size_t(floor(offset / this->block_size));
  auto offset_in_block = offset % this->block_size;
  size_t written = 0;
  if (block_index < blocks->size()) {
      vector<char> *block = (*blocks)[block_index];
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
    vector<char> *block = new vector<char>(bytes_to_write);
    memcpy(block->data(), data + written, bytes_to_write);
    blocks->push_back(block);
    written += bytes_to_write;
  }
  auto end = chrono::high_resolution_clock::now();
  auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);
  return written;
}

int FileSystem::read_data(const vector<vector< char>*>* blocks,
                          char *buffer,
                          const size_t block_index,
                          const size_t offset,
                          const size_t size) {
  size_t read = 0;
  size_t offset_in_block = offset % this->block_size;
  for (size_t index = block_index;
       index < blocks->size() && read < size;
       index++, offset_in_block = 0) {
    vector<char> *block = blocks->at(index);
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
  string filename = clean_path(path);
  auto start = chrono::high_resolution_clock::now();
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
  auto end = chrono::high_resolution_clock::now();
  auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);
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

vector<string> FileSystem::readdir(const string &path) {
  string pathname = clean_path(path);
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
