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
    return -EEXIST;
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
  string message = "About to list content of " + pathname;
  ocall_print(message.c_str());
  if (!pathname.empty() && this->directories->find(pathname) == this->directories->end()) {
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
  std::string message = "getting the number of entries for " + pathname;
  ocall_print(message.c_str());
  if (!pathname.empty() && !this->is_directory(pathname)) {
    return -ENOENT;
  }
  ocall_print("Directory exists");
  return this->readdir(pathname).size();
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

bool FileSystem::is_file(const std::string &path) const {
  std::string cleaned_path = clean_path(path);
  return this->files->find(cleaned_path) != this->files->end();
}


bool FileSystem::is_directory(const std::string &path) const {
  std::string cleaned_path = clean_path(path);
  return this->directories->find(cleaned_path) != this->directories->end();
}

bool FileSystem::exists(const std::string &path) const {
  string cleaned_path = clean_path(path);
  if (this->directories->find(cleaned_path) != this->directories->end() ||
      this->files->find(cleaned_path) != this->files->end()) {
    return true;
  }
  return false;
}

static FileSystem* FILE_SYSTEM;

static string strip_leading_slash(string filename) {
  if (filename.size() > 0 && filename[0] == '/') {
    return filename.substr(1, string::npos);
  }
  return filename;
}

int enclave_is_file(const char* filename) {
  string cleaned_path = FileSystem::clean_path(filename);
  string message = "Checking if " + cleaned_path + " exists";
  ocall_print(message.c_str());
  if (FILE_SYSTEM->is_file(cleaned_path)) {
    message = cleaned_path + " is a file";
    ocall_print(message.c_str());
    return EEXIST;
  }
  if (cleaned_path.empty() || FILE_SYSTEM->is_directory(cleaned_path)) {
    message = cleaned_path + " is a directory!";
    ocall_print(message.c_str());
    return -EISDIR;
  }
  message = cleaned_path + " is not a file nor a directory!";
  ocall_print(message.c_str());
  return -ENOENT;
}

int ramfs_get(const char* filename,
              long offset,
              size_t size,
              char* buffer) {
  std::string cleaned_path = FileSystem::clean_path(filename);
  return FILE_SYSTEM->read(cleaned_path, buffer, offset, size);
}

int ramfs_put(const char *filename,
              long offset,
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
  ocall_print("Getting the number of entries");
  return FILE_SYSTEM->get_number_of_entries("/");
}

int enclave_readdir(char*entries, size_t length) {
  size_t i = 0;
  const size_t offset = 256;
  size_t number_of_entries = 0;
  ocall_print("called readdir on /");
  std::vector<std::string> files = FILE_SYSTEM->readdir("/");
  std::string message = "";
  for (auto it = files.begin();
       it != files.end() && i < length;
       it++, number_of_entries++) {
    string name = (*it);
    message += name + '\n';
    memcpy(entries + i, name.c_str(), name.length());
    entries[i + name.length()] = 0x1C;
    i += name.length() + 1;
  }
  ocall_print(message.c_str());
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
  sgx_status_t status = sgx_unseal_data(encrypted, NULL, NULL, (uint8_t*) plaintext, &data_size);
  return status;
}


int sgxfs_dump(const char* pathname,
               sgx_sealed_data_t* sealed_data,
               size_t sealed_size) {
  string path = FileSystem::clean_path(pathname);
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
  string path = FileSystem::clean_path(pathname);
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
  return FILE_SYSTEM->mkdir(string(pathname));
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
