#include <cerrno>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Enclave_t.h"

using namespace std;


static map<string, vector<char> > files;

static string strip_leading_slash(string filename) {
  bool starts_with_slash = false;

  if (filename.size() > 0 && filename[0] == '/') {
      starts_with_slash = true;
  }

  return starts_with_slash ? filename.substr(1, string::npos) : filename;
}

int ramfs_file_exists(const char* filename) {
  string name(filename);
  return files.find(filename) != files.end() ? 1 : 0;
}

int ramfs_get(const char* filename,
              long offset,
              size_t size,
              char* buffer) {
  ocall_print("[ramfs_get] Entering");
  vector<char> &file = files[filename];

  size_t len = file.size();
  if (offset < len) {
    if (offset + size > len) {
      size = len - offset;
    }
    for (size_t i = 0; i < size; i++) {
      buffer[i] = file[offset + i];
    }
    ocall_print("[ramfs_get] Exiting");
    return size;
  }
  return -EINVAL;
}

int ramfs_put(const char *filename,
              long offset,
              size_t size,
              const char *data) {
  ocall_print("[ramfs_put] entering");
  string path = strip_leading_slash(filename);
  if (!ramfs_file_exists(path.c_str())) {
    return -ENOENT;
  }
  ocall_print("[ramfs_put] About to copy data!");
  vector<char> &file = files[filename];
  //Check if the byte vector needs to be resized before writing to it
  if (file.capacity() < (offset + size)) {
    size_t size_difference = (offset + size) - file.capacity();
    file.resize(size_difference);
  }
  for (size_t i = 0; i < size; i++) {
    file[offset + i] = data[i];
  }
  ocall_print("[ramfs_put] exiting");
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

int ramfs_list_entries(char** entries) {
  int number_of_entries = ramfs_get_number_of_entries();
  size_t i = 0;
  for (map<string, vector<char> >::iterator it = files.begin(); it != files.end() && i < number_of_entries; it++, i++) {
    string name = it->first;
    entries[i] = new char[name.length() + 1];
    name.copy(entries[i], sizeof(entries[i]));
    entries[i][name.length()] = '\0';
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
  ocall_print("Processing random number generation...");
  if ((number % 2) == 0) {
    number /= 2;
  } else {
    number = (number * 3) + 1;
  }
  return number;
}
