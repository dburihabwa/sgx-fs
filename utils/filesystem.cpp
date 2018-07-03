#include "filesystem.hpp"

#include <climits>

#include <cmath>
#include <cstring>

#include <chrono>
#include <map>
#include <string>
#include <vector>

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
  if (this->directories->find(path) != this->directories->end()) {
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

int FileSystem::write(const std::string &path, const char *data, const size_t offset, const size_t length) {
  std::string filename = clean_path(path);
  auto start = std::chrono::high_resolution_clock::now();
  auto entry = this->files->find(filename);
  if (entry == this->files->end()) {
      return -ENOENT;
  }
  auto blocks = entry->second;
  auto block_index = size_t(floor(offset / this->block_size));
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
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  return written;
}


size_t FileSystem::get_file_size(const std::string &path) const {
  std::string cleaned_path = clean_path(path);
  auto entry = this->files->find(cleaned_path);
  if (entry == this->files->end()) {
    return -1;
  }
  auto blocks = entry->second;
  if (blocks->empty()) {
    return 0;
  }
  size_t size = (blocks->size() - 1) * this->block_size;
  size += blocks->back()->size();
  return size;
}

int FileSystem::truncate(const std::string &path, const size_t length) {
  std::string filename = clean_path(path);
  auto len = static_cast<unsigned int>(length);
  auto entry = this->files->find(filename);
  if (entry == this->files->end()) {
    return -ENOENT;
  }

  auto blocks = entry->second;
  auto file_size = this->get_file_size(filename);
  if (file_size == len) {
    return 0;
  }
  if (file_size < len) {
    auto length_difference = len - blocks->size();
    auto blocks_to_add = static_cast<size_t>(floor(length_difference / this->block_size));
    if (blocks_to_add > 0) {
      for (size_t i = 0; i < blocks_to_add; i++) {
        blocks->push_back(new std::vector<char>(this->block_size));
      }
    }
    auto length_of_last_block = len % this->block_size;
    if (length_of_last_block > 0) {
      blocks->push_back(new std::vector<char>(length_of_last_block));
    }
    return 0;
  }
  auto blocks_to_keep = static_cast<unsigned int>(ceil(len / this->block_size));
  while (blocks_to_keep < blocks->size()) {
    delete blocks->back();
    blocks->pop_back();
  }
  if (blocks->empty()) {
    return 0;
  }

  auto block_to_trim = blocks->back();
  auto bytes_to_keep = len % this->block_size;
  block_to_trim->resize(bytes_to_keep);

  return 0;
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
  auto start = std::chrono::high_resolution_clock::now();
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
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  return read;
}

int FileSystem::mkdir(const std::string &path) {
  string directory = FileSystem::clean_path(path);
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

int FileSystem::get_number_of_entries(const std::string &directory) const {
  std::string pathname = clean_path(directory);
  if (!this->is_directory(pathname)) {
    return -ENOENT;
  }
  size_t count = 0;
  pathname += "/";
  for (auto it = this->files->lower_bound(pathname); it != this->files->end(); it++) {
    std::string filename = it->first;
    if (filename.compare(0, pathname.size(), pathname) != 0) {
      break;
    }
    count++;
  }
  return count;
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

/**
 * Returns a full path to the library
 * @param path Path to interpret
 * @return Absolute path
 */
std::string FileSystem::get_absolute_path(const std::string &path) {
  char absolute_path[PATH_MAX];
  realpath(path.c_str(), absolute_path);
  return std::string(absolute_path);
}

std::string FileSystem::get_directory(const std::string &path) {
  std::string absolute_path = get_absolute_path(path);
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

bool FileSystem::is_file(const std::string &path) const {
  std::string cleaned_path = clean_path(path);
  return this->files->find(cleaned_path) != this->files->end();
}

bool FileSystem::is_directory(const std::string &path) const {
  std::string cleaned_path = clean_path(path);
  return this->directories->find(cleaned_path) != this->directories->end();
}

bool FileSystem::exists(const std::string &path) const {
  return this->is_directory(path) || this->is_file(path);
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
