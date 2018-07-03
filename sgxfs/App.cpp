/**
 * Code directly taken and modified from Dennis Pfisterer's fuse-ramfs
 * https://github.com/pfisterer/fuse-ramfs
 */

#define FUSE_USE_VERSION 26

#include <fcntl.h>
#include <fuse.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "Enclave_u.h"
#include "./sgx_urts.h"
#include "sgx_utils/sgx_utils.h"
#include "../utils/fs.hpp"
#include "../utils/serialization.hpp"
#include "../utils/logging.h"

using namespace std;

static sgx_enclave_id_t ENCLAVE_ID;
static char* BINARY_NAME;

void ocall_print(const char* str) {
  printf("[ocall_print] %s\n", str);
}

static int sgxfs_getattr(const char *path, struct stat *stbuf) {
  string filename = path;
  string stripped_slash = strip_leading_slash(filename);
  int res = 0;
  memset(stbuf, 0, sizeof(struct stat));

  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

  int found;
  sgx_status_t status = enclave_is_file(ENCLAVE_ID, &found, stripped_slash.c_str());

  if (found == EEXIST) {
    stbuf->st_mode = S_IFREG | 0777;
    stbuf->st_nlink = 1;
    int file_size;
    sgx_status_t status = ramfs_get_size(ENCLAVE_ID, &file_size, stripped_slash.c_str());
    stbuf->st_size = file_size;
  } else if (found == -EISDIR) {
    stbuf->st_mode = S_IFDIR | 0777;
    stbuf->st_nlink = 2;
  } else {
    res = -ENOENT;
  }
  return res;
}

static std::vector<std::string> tokenize(const string list_of_entries, const char separator) {
  string entries(list_of_entries);
  std::vector<std::string> filenames;
  size_t pos = 0;
  size_t found = string::npos;
  while (true) {
    found = entries.find(separator, pos);
    if (found == string::npos) {
      break;
    }
    size_t length = found - pos;
    if (length == 0) {
      pos = found + 1;
      continue;
    }
    string filename = entries.substr(pos, length);
    filenames.push_back(filename);
    pos = found + 1;
  }
  return filenames;
}

static int sgxfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
  int ret;
  enclave_is_file(ENCLAVE_ID, &ret, path);
  if (ret == -ENOENT) {
    return -ENOENT;
  }
  if (ret == EEXIST) {
    return -ENOTDIR;
  }

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  int number_of_entries;
  ramfs_get_number_of_entries(ENCLAVE_ID, &number_of_entries);
  const size_t step = 256;
  const size_t buffer_length = number_of_entries * step;
  char *entries = new char[buffer_length];
  int size;
  enclave_readdir(ENCLAVE_ID, &size, path, entries, buffer_length);
  vector<string> filenames = tokenize(string(entries), 0x1C);
  for (auto it = filenames.begin(); it != filenames.end(); it++) {
    filler(buf, it->c_str(), NULL, 0);
  }
  if (entries != NULL) {
    delete [] entries;
  }
  return 0;
}

static int sgxfs_open(const char *path, struct fuse_file_info *fi) {
  string filename = strip_leading_slash(path);
  int found;
  sgx_status_t status = enclave_is_file(ENCLAVE_ID, &found, filename.c_str());

  if (found == -ENOENT) {
    cerr << "sgxfs_open(" << filename << "): Not found" << endl;
    return -ENOENT;
  }

  return 0;
}

static int sgxfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  string filename = strip_leading_slash(path);

  int found;
  sgx_status_t status = enclave_is_file(ENCLAVE_ID, &found, filename.c_str());

  if (found == -ENOENT) {
    cerr << "[sgxfs_read] " << filename << ": Not found" << endl;
    return -ENOENT;
  }

  int read;
  status = ramfs_get(ENCLAVE_ID, &read, filename.c_str(), (long) offset, size, buf);
  return read;
}

int sgxfs_write(const char *path, const char *data, size_t size, off_t offset,
                struct fuse_file_info *) {
  string filename = strip_leading_slash(path);

  int found;
  sgx_status_t status = enclave_is_file(ENCLAVE_ID, &found, filename.c_str());
  if (found == -ENOENT) {
    cerr << "[sgxfs_write] " << filename << ": Not found" << endl;
    return -ENOENT;
  }

  int written;
  status = ramfs_put(ENCLAVE_ID, &written, filename.c_str(), (long) offset, size, data);

  return written;
}

int sgxfs_unlink(const char *pathname) {
  string filename = strip_leading_slash(pathname);
  int retval;
  sgx_status_t status = ramfs_delete_file(ENCLAVE_ID, &retval, filename.c_str());
  return retval;
}

int sgxfs_create(const char *path, mode_t mode, struct fuse_file_info *) {
  string filename = strip_leading_slash(path);

  int found;
  sgx_status_t status = enclave_is_file(ENCLAVE_ID, &found, filename.c_str());
  if (found != -ENOENT) {
    cerr << "sgxfs_create(" << filename << "): Already exists" << endl;
    return -EEXIST;
  }

  if ((mode & S_IFREG) == 0) {
    cerr << "sgxfs_create(" << filename << "): Only files may be created"
         << endl;
    return -EINVAL;
  }
  int retval;
  sgx_status_t stattus = ramfs_create_file(ENCLAVE_ID, &retval, filename.c_str());
  return retval;
}

int sgxfs_fgetattr(const char *path, struct stat *stbuf,
                   struct fuse_file_info *) {
  return sgxfs_getattr(path, stbuf);
}

int sgxfs_opendir(const char *path, struct fuse_file_info *) {
  return 0;
}

int sgxfs_access(const char *path, int) {
  return 0;
}

int sgxfs_truncate(const char *path, off_t length) {
  string filename = strip_leading_slash(path);

  int found;
  sgx_status_t status = enclave_is_file(ENCLAVE_ID, &found, filename.c_str());
  if (found == -ENOENT) {
    cerr << "sgxfs_truncate(" << filename << "): Not found" << endl;
    return -ENOENT;
  }

  int retval;
  sgx_status_t stattus = ramfs_trunkate(ENCLAVE_ID, &retval,  filename.c_str(), length);

  return retval;
}

int sgxfs_mknod(const char *path, mode_t mode, dev_t dev) {
  cout << "sgxfs_mknod not implemented" << endl;
  return -EINVAL;
}
int sgxfs_mkdir(const char* pathname, mode_t unused_mode) {
  int retval;
  enclave_mkdir(ENCLAVE_ID, &retval, pathname);
  return retval;
}
int sgxfs_rmdir(const char *) {
  cout << "sgxfs_rmdir not implemented" << endl;
  return -EINVAL;
}
int sgxfs_symlink(const char *, const char *) {
  cout << "sgxfs_symlink not implemented" << endl;
  return -EINVAL;
}
int sgxfs_rename(const char *, const char *) {
  cout << "sgxfs_rename not implemented" << endl;
  return -EINVAL;
}
int sgxfs_link(const char *, const char *) {
  cout << "sgxfs_link not implemented" << endl;
  return -EINVAL;
}
int sgxfs_chmod(const char *, mode_t) {
  cout << "sgxfs_chmod not implemented" << endl;
  return -EINVAL;
}
int sgxfs_chown(const char *, uid_t, gid_t) {
  cout << "sgxfs_chown not implemented" << endl;
  return -EINVAL;
}
int sgxfs_utime(const char *, struct utimbuf *) {
  cout << "sgxfs_utime not implemented" << endl;
  return -EINVAL;
}
int sgxfs_utimens(const char *, const struct timespec tv[2]) {
  cout << "sgxfs_utimens not implemented" << endl;
  return 0;
}
int sgxfs_bmap(const char *, size_t blocksize, uint64_t *idx) {
  cout << "sgxfs_bmap not implemented" << endl;
  return -EINVAL;
}
int sgxfs_setxattr(const char *, const char *, const char *, size_t, int) {
  cout << "sgxfs_setxattr not implemented" << endl;
  return -EINVAL;
}

static void restore_fs(const int enclave_id, const string &directory) {
  map<string, sgx_sealed_data_t*>* restored_files = restore_sgxfs_from_disk("sgxfs_dump");
  for (auto it = restored_files->begin(); it != restored_files->end();) {
    const char* filename = it->first.c_str();
    sgx_sealed_data_t* sealed_file = it->second;
    size_t sealed_size = sizeof(sgx_sealed_data_t) + sealed_file->aes_data.payload_size;
    int ret;
    sgx_status_t status = sgxfs_restore(enclave_id, &ret, filename, sealed_file, sealed_size);
    restored_files->erase(it++);
    free(sealed_file);
  }
  delete restored_files;
}

void* sgxfs_init(struct fuse_conn_info *conn) {
  Logger init_log("sgxfs-mount.log");
  chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();
  string binary_directory = get_directory(string(BINARY_NAME));
  string path_to_enclave_token = binary_directory + "/enclave.token";
  string path_to_enclave_so = binary_directory + "/enclave.signed.so";
  if (initialize_enclave(&ENCLAVE_ID,
                         path_to_enclave_token,
                         path_to_enclave_so) < 0) {
      // LOGGER.error("Fail to initialize enclave.");
      exit(1);
  }
  int ret;
  init_filesystem(ENCLAVE_ID, &ret);
  restore_fs(ENCLAVE_ID, "sgxfs_dump");
  chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
  init_log.info("Mounted in " + to_string(duration) + " nanoseconds");
  return &ENCLAVE_ID;
}

static void dump_fs(const string &path) {
  int number_of_entries;
  sgx_status_t status = ramfs_get_number_of_entries(ENCLAVE_ID, &number_of_entries);
  const size_t step = 256;
  const size_t buffer_length = number_of_entries * step;
  char *entries = new char[buffer_length];
  int size;
  status = enclave_readdir(ENCLAVE_ID, &size, "/", entries, buffer_length);

  for (size_t i = 0; i < buffer_length; i += step) {
    char* entry = entries + (i * sizeof(char));
    string pathname(entry);
    int file_size;
    status = ramfs_get_size(ENCLAVE_ID, &file_size, pathname.c_str());
    size_t sealed_size = sizeof(sgx_sealed_data_t) + file_size;
    sgx_sealed_data_t* sealed_data = reinterpret_cast<sgx_sealed_data_t*>(malloc(sealed_size));
    int ret;
    status = sgxfs_dump(ENCLAVE_ID,
                        &ret,
                        pathname.c_str(),
                        sealed_data,
                        sealed_size);
    string dump_pathname = path + "/" + pathname;
    dump(reinterpret_cast<char*>((sealed_data)), dump_pathname, sealed_size);
    free(sealed_data);
  }
  delete [] entries;
}

void sgxfs_destroy(void* private_data) {
  // FIXME(dburihabwa) segmentation fault on call to destroy
  Logger init_log("sgxfs-mount.log");
  chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();
  dump_fs("sgxfs_dump");
  int ret;
  destroy_filesystem(ENCLAVE_ID, &ret);
  sgx_destroy_enclave(ENCLAVE_ID);
  chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
  init_log.info("Unmounted in " + to_string(duration) + " nanoseconds");
}

static struct fuse_operations sgxfs_oper;

int main(int argc, char **argv) {
  BINARY_NAME = argv[0];
  sgxfs_oper.getattr = sgxfs_getattr;
  sgxfs_oper.readdir = sgxfs_readdir;
  sgxfs_oper.open = sgxfs_open;
  sgxfs_oper.read = sgxfs_read;
  sgxfs_oper.mknod = sgxfs_mknod;
  sgxfs_oper.write = sgxfs_write;
  sgxfs_oper.unlink = sgxfs_unlink;

  sgxfs_oper.setxattr = sgxfs_setxattr;
  sgxfs_oper.mkdir = sgxfs_mkdir;
  sgxfs_oper.rmdir = sgxfs_rmdir;
  sgxfs_oper.symlink = sgxfs_symlink;
  sgxfs_oper.rename = sgxfs_rename;
  sgxfs_oper.link = sgxfs_link;
  sgxfs_oper.chmod = sgxfs_chmod;
  sgxfs_oper.chown = sgxfs_chown;
  sgxfs_oper.truncate = sgxfs_truncate;
  sgxfs_oper.utime = sgxfs_utime;
  sgxfs_oper.opendir = sgxfs_opendir;
  sgxfs_oper.access = sgxfs_access;
  sgxfs_oper.create = sgxfs_create;
  sgxfs_oper.fgetattr = sgxfs_fgetattr;
  sgxfs_oper.utimens = sgxfs_utimens;
  sgxfs_oper.bmap = sgxfs_bmap;

  sgxfs_oper.init = sgxfs_init;
  sgxfs_oper.destroy = sgxfs_destroy;

  return fuse_main(argc, argv, &sgxfs_oper, NULL);
}
