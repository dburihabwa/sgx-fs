#include "serialization.hpp"

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <stdexcept>
#include <vector>


#include "fs.hpp"


void dump(const char *data, const std::string &path, size_t bytes) {
  std::ofstream stream;
  stream.open(path, std::ios::out | std::ios::binary);
  stream.write(data, bytes);
  stream.close();
}

static bool is_a_directory(const std::string &path) {
  struct stat info;
  char* pathname = const_cast<char*>(path.c_str());
  if (stat(pathname, &info) != 0) {
      return false;
  } else if (info.st_mode & S_IFDIR) {
      return true;
  }
  return false;
}

static void make_parent_directory(const std::string &path) {
  vector<string>* tokens = split_path(path);
  string current_path;
  for (size_t i = 0; i < tokens->size() - 1; i++) {
    current_path += tokens->at(i) + "/";
    if (!is_a_directory(current_path)) {
      mkdir(current_path.c_str(), S_IRWXU);
    }
  }
  delete tokens;
}

static void make_directory(const std::string &path) {
  string new_path = path + "/ignored";
  make_parent_directory(new_path);
}

void dump_map(const std::map<std::string, std::vector < std::vector < char>*>*> &files, const std::string &directory_path) {
  if (!is_a_directory(directory_path)) {
    make_directory(directory_path);
  }
  for (auto it = files.begin(); it != files.end(); it++) {
    std::vector<std::vector<char>*>* blocks = it->second;
    std::vector<char> buffer;
    for (auto b = blocks->begin(); b != blocks->end(); b++) {
      std::vector<char>* block = (*b);
      buffer.insert(buffer.end(), block->begin(), block->end());
    }
    const std::string dump_path = directory_path + "/" + it->first;
    make_parent_directory(dump_path);

    dump(buffer.data(), dump_path, buffer.size());
  }
}

size_t restore(const std::string &path, char* buffer) {
  std::ifstream stream;
  stream.open(path, std::ios::binary);
  stream.seekg(0, std::ios::end);
  size_t size = stream.tellg();
  stream.seekg(stream.beg);
  if (buffer != NULL) {
    delete[] buffer;
  }
  buffer = new char[size];
  stream.read(buffer, size);
  stream.close();
  cout << "restore(" << path << ") = "<< buffer << endl;
  return size;
}

// TODO(dburihabwa) Return a vector rather than a pointer
static vector<string>* list_files(const std::string &path) {
  std::vector<string>* files = new std::vector<string>();
  DIR* directory = opendir(path.c_str());
  struct dirent* entry;
  while ((entry = readdir(directory)) != NULL) {
    string entry_name(entry->d_name);
    if (entry_name.compare(".") == 0 || entry_name.compare("..") == 0) {
      continue;
    }
    if (entry->d_type == DT_DIR) {
      std::string directory_name = path + "/" + entry_name;
      std::vector<string>* files_in_dir = list_files(directory_name);
      files->insert(files->end(), files_in_dir->begin(), files_in_dir->end());
      delete files_in_dir;
    } else {
      std::string filename = path + "/" + entry_name;
      files->push_back(filename);
    }
  }
  return files;
}

std::map<std::string, vector<vector<char>*>*>* restore_map(const std::string &path) {
  if (!is_a_directory(path)) {
    make_directory(path);
  }
  const size_t BLOCK_SIZE = 4096;
  auto files = new std::map<std::string, vector<vector<char>*>*>();
  auto filenames = list_files(path);
  for (auto it = filenames->begin(); it != filenames->end(); it++) {
    string filename = (*it);
    std::ifstream stream;
    stream.open(filename, std::ios::binary);
    stream.seekg(0, std::ios::end);
    size_t restored = stream.tellg();
    stream.seekg(stream.beg);
    char* buffer = new char[restored];
    stream.read(buffer, restored);
    stream.close();
    auto blocks = new vector<vector<char>*>();
    for (size_t offset = 0; offset < restored; offset += BLOCK_SIZE) {
      size_t bytes_to_copy = restored - offset;
      if (bytes_to_copy > BLOCK_SIZE) {
        bytes_to_copy = BLOCK_SIZE;
      }
      auto block = new std::vector<char>(buffer + offset, buffer + offset + bytes_to_copy);
      blocks->push_back(block);
    }
    delete [] buffer;
    filename = clean_path(filename.substr(path.length(), string::npos));
    (*files)[filename] = blocks;
  }
  return files;
}

void dump_sgx_map(const std::map<std::string, std::vector<sgx_sealed_data_t*>*> &files,
                  const std::string &directory_path) {
  if (!is_a_directory(directory_path)) {
    make_directory(directory_path);
  }
  for (auto it = files.begin(); it != files.end(); it++) {
    std::vector<sgx_sealed_data_t*>* blocks = it->second;
    size_t counter = 0;
    for (auto b = blocks->begin(); b != blocks->end(); b++, counter++) {
      sgx_sealed_data_t* block = (*b);
      size_t payload_size = sizeof(sgx_sealed_data_t) +
                            block->aes_data.payload_size;
      const std::string dump_path = directory_path + "/" + it->first + "-" +
                                    to_string(counter);
      make_parent_directory(dump_path);
      dump(reinterpret_cast<char*>(block), dump_path, payload_size);
    }
  }
}

static map<string, vector<string>> group_blocks_by_file(const vector<string> &blocks) {
  map<string, vector<string>> groups;
  for (auto it = blocks.begin(); it != blocks.end(); it++) {
    string block = (*it);
    auto index_separator = block.find_last_of("-");
    if (index_separator == string::npos) {
      throw runtime_error("Could not find index separator '-' in " + block);
    }
    string filename = clean_path(block.substr(0, index_separator));
    auto entry = groups.find(filename);
    vector<string> grouped_blocks;
    if (entry != groups.end()) {
      grouped_blocks = entry->second;
    }
    grouped_blocks.push_back(block);
    groups[filename] = grouped_blocks;
  }
  return groups;
}

std::map<std::string, std::vector<sgx_sealed_data_t*>*>* restore_sgx_map(const std::string &path) {
  if (!is_a_directory(path)) {
    make_directory(path);
  }
  auto files_on_disk = list_files(path);
  auto grouped_blocks = group_blocks_by_file((*files_on_disk));
  auto files = new std::map<std::string, vector<sgx_sealed_data_t*>*>();
  delete files_on_disk;
  for (auto it = grouped_blocks.begin(); it != grouped_blocks.end(); it++) {
    string filename = it->first;
    auto blocks = it->second;
    auto sealed_blocks = new vector<sgx_sealed_data_t*>();
    for (auto b = blocks.begin(); b != blocks.end(); b++) {
      auto block_path = (*b);
      std::ifstream stream;
      stream.open(block_path, std::ios::binary);
      stream.seekg(0, std::ios::end);
      size_t restored = stream.tellg();
      stream.seekg(stream.beg);
      auto block = reinterpret_cast<sgx_sealed_data_t*>(malloc(restored));
      stream.read(reinterpret_cast<char*>(block), restored);
      stream.close();
      sealed_blocks->push_back(block);
    }
    filename = clean_path(filename.substr(path.length(), string::npos));

    (*files)[filename] = sealed_blocks;
  }
  return files;
}

std::map<std::string, sgx_sealed_data_t*>* restore_sgxfs_from_disk(const std::string &path) {
  if (!is_a_directory(path)) {
    make_directory(path);
  }
  auto files_on_disk = list_files(path);
  auto files = new std::map<std::string, sgx_sealed_data_t*>();
  for (auto it = files_on_disk->begin(); it != files_on_disk->end(); it++) {
    string filename = (*it);
    std::ifstream stream;
    stream.open(filename, std::ios::binary);
    stream.seekg(0, std::ios::end);
    size_t restored = stream.tellg();
    stream.seekg(stream.beg);
    sgx_sealed_data_t* data = reinterpret_cast<sgx_sealed_data_t*>(malloc(restored));
    stream.read(reinterpret_cast<char*>(data), restored);
    stream.close();
    filename = clean_path(filename.substr(path.length(), string::npos));
    (*files)[filename] = data;
  }
  delete files_on_disk;
  return files;
}
