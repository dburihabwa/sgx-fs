#include <cerrno>
#include <map>
#include <string>

#include "Enclave_t.h"

using namespace std;


typedef std::map<int, unsigned char> FileContents;
typedef std::map<string, FileContents> FileMap;
static FileMap files;

static string strip_leading_slash(string filename) {
  bool starts_with_slash = false;

  if (filename.size() > 0 && filename[0] == '/') {
      starts_with_slash = true;
  }

  return starts_with_slash ? filename.substr(1, string::npos) : filename;
}

static FileContents to_map(string data) {
  FileContents data_map;
  int i = 0;

  for (string::iterator it = data.begin(); it < data.end(); ++it)
    data_map[i++] = *it;

  return data_map;
}

int ramfs_file_exists(const char* filename) {
  string name(filename);
  return files.find(filename) != files.end() ? 1 : 0;
}

int ramfs_get(const char* filename,
              unsigned offset,
              unsigned size,
              char* buffer) {
  FileContents &file = files[filename];
  size_t len = file.size();
  if (offset < len) {
    if (offset + size > len) {
      size = len - offset;
    }
    for (size_t i = 0; i < size; ++i) {
      buffer[i] = file[offset + i];
    }
    return size;
  }
  return -EINVAL;
}

int ramfs_put(const char *filename,
              size_t offset,
              size_t size,
              const char *data) {
  string path = strip_leading_slash(filename);
  if (!ramfs_file_exists(path.c_str())) {
    return -ENOENT;
  }
  FileContents &file = files[filename];
  for (size_t i = 0; i < size; ++i) {
    file[offset + i] = data[i];
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
  FileContents &file = files[filename];
  if (file.size() >= length) {
    file.erase(file.find(length), file.end());
    return 0;
  }
  for (size_t i = file.size(); i < length; ++i) {
    file[i] = '\0';
  }
  return 0;
}

int ramfs_list_entries(char** entries) {
  if (entries != NULL) {
    free(entries);
  }
  int number_of_entries = files.size() + 2;
  entries = (char**) malloc(sizeof(char*) * number_of_entries);
  entries[0] = ".";
  entries[1] = "..";
  size_t i = 2;
  for (FileMap::iterator it = files.begin(); it != files.end() && i < number_of_entries; it++, i++) {
    string name = it->first;
    entries[i] = new char[name.length() + 1];
    name.copy(entries[i], sizeof(entries[i]));
    entries[i][name.length()] = '\0';
  }
  return number_of_entries;
}

int ramfs_create_file(const char *path) {
  string filename = strip_leading_slash(path);
  files[filename] = to_map("");
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
