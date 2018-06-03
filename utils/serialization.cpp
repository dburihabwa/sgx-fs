#include "serialization.hpp"

#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
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

size_t restore(const std::string &path, char *buffer) {
  std::ifstream stream;
  stream.open(path);
  stream.seekg(stream.end);
  size_t size = stream.tellg();
  stream.seekg(stream.beg);
  if (buffer != NULL) {
    delete [] buffer;
  }
  buffer = new char[size];
  stream.read(buffer, size);
  stream.close();
  return size;
}
