#include "serialization.hpp"

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cstring>
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

void dump_map(const std::map<std::string, std::vector < std::vector < char>*>*> &files, const std::string &directory_path) {
  if (!is_a_directory(directory_path)) {
    throw new std::runtime_error(directory_path + " is not a valid directory!");
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
    throw new runtime_error(path + " is not a directory!");
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
      auto block = new std::vector<char>(buffer, buffer + offset + bytes_to_copy);
      blocks->push_back(block);
    }
    delete [] buffer;
    filename = clean_path(filename.substr(path.length(), string::npos));
    (*files)[filename] = blocks;
  }
  return files;
}
