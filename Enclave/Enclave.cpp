#include <cerrno>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Enclave_t.h"

using namespace std;


static map<string, vector<char> > files;

static string strip_leading_slash(string filename) {
  if (filename.size() > 0 && filename[0] == '/') {
    return filename.substr(1, string::npos);
  }
  return filename;
}

int ramfs_file_exists(const char* filename) {
  string name(filename);
  return files.find(filename) != files.end() ? 1 : 0;
}

int ramfs_get(const char* filename,
              long offset,
              size_t size,
              char* buffer) {
  vector<char> &file = files[filename];

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
  vector<char> *file = &(files[filename]);
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
  return files[strip_leading_slash(pathname)].size();
}

int ramfs_trunkate(const char* path, size_t length) {
  if (!ramfs_file_exists(path)) {
    return -ENOENT;
  }
  string filename = strip_leading_slash(path);
  vector<char> &file = files[filename];
  file.resize(length);
  return 0;
}

int ramfs_get_number_of_entries() {
  return files.size();
}

int ramfs_list_entries(char*entries, size_t length) {
  size_t i = 0;
  const size_t offset = 256;
  size_t number_of_entries = 0;
  for (auto it = files.begin(); it != files.end() && i < length; it++, i += offset, number_of_entries++) {
    string name = it->first;
    //memcpy seems to do the trick bug str::cpy fails miserably here
    memcpy(entries + i, name.c_str(), name.length());
    entries[i + name.length()] = '\0';
  }
  return number_of_entries;
}

int ramfs_create_file(const char *path) {
  string filename = strip_leading_slash(path);
  files[filename] = vector<char>();
  return 0;
}

int ramfs_delete_file(const char *pathname) {
  files.erase(strip_leading_slash(pathname));
  return 0;
}

int generate_random_number() {
  static int number = 42;
  if ((number % 2) == 0) {
    number /= 2;
  } else {
    number = (number * 3) + 1;
  }
  return number;
}
