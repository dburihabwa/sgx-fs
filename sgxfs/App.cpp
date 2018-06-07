/**
 * Code directly taken and modified from Dennis Pfisterer's fuse-ramfs
 * https://github.com/pfisterer/fuse-ramfs
 */
#include <iostream>
#include <string>
using namespace std;

#include "Enclave_u.h"
#include "sgx_urts.h"
#include "sgx_utils/sgx_utils.h"

static string strip_leading_slash(string filename) {
  bool starts_with_slash = false;

  if (filename.size() > 0)
    if (filename[0] == '/')
      starts_with_slash = true;

  return starts_with_slash ? filename.substr(1, string::npos) : filename;
}

sgx_enclave_id_t ENCLAVE_ID;

extern "C" {

#define FUSE_USE_VERSION 26
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

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

  if (filename == "/") {
    cout << "sgxfs_getattr(" << filename << "): Returning attributes for /"
         << endl;
    stbuf->st_mode = S_IFDIR | 0777;
    stbuf->st_nlink = 2;

  } else {
    int found;
    sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, stripped_slash.c_str());
    if (found) {
      stbuf->st_mode = S_IFREG | 0777;
      stbuf->st_nlink = 1;
      int file_size;
      sgx_status_t status = ramfs_get_size(ENCLAVE_ID, &file_size, stripped_slash.c_str());
      stbuf->st_size = file_size;

    } else {
      res = -ENOENT;
    }
  }
  return res;
}

static int sgxfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
  cout << "[entering] sgxfs_readdir" << endl;
  if (strcmp(path, "/") != 0) {
    cout << "sgxfs_readdir(" << path << "): Only / allowed" << endl;
    return -ENOENT;
  }
  cout << "[sgxfs_readdir] adding basic entries" << endl;
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  cout << "[sgxfs_readdir] added basic entries" << endl;

  int number_of_entries;
  cout << "[sgxfs_readdir] sgx in" << endl;
  sgx_status_t status = ramfs_get_number_of_entries(ENCLAVE_ID, &number_of_entries);
  cout << "[sgxfs_readdir] sgx out expecting " << number_of_entries << " entries" << endl;
  const size_t step = 256;
  const size_t buffer_length = number_of_entries * step;
  char *entries = new char[buffer_length];
  cout << "Initializing buffer of length " << buffer_length << endl;
  int size;
  cout << "[sgxfs_readdir] sgx in" << endl;
  status = ramfs_list_entries(ENCLAVE_ID, &size, entries, buffer_length);
  cout << "[sgxfs_readdir] sgx out with " << size << " entries of size " << sizeof(entries[0]) << endl;
  cout << "[sgxfs_readdir] sgx out with " << entries << endl;

  for (int i = 0; i < buffer_length; i += step) {
    char* entry = entries + (i * sizeof(char));
    cout << "[sgxfs_readdir] adding " << entry << " to the list (length = " << strlen(entry) << ")" << endl;
    filler(buf, entry, NULL, 0);
    cout << "[sgxfs_readdir] added  " << entry << " to the list" << endl;
  }
  cout << "[sgxfs_readdir] freeing out data structures" << endl;
  if (entries != NULL) {
    delete(entries);
  }
  cout << "[exiting] sgxfs_readdir" << endl;
  return 0;
}

static int sgxfs_open(const char *path, struct fuse_file_info *fi) {
  string filename = strip_leading_slash(path);
  int found;
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());

  if (!found) {
    cout << "sgxfs_open(" << filename << "): Not found" << endl;
    return -ENOENT;
  }

  return 0;
}

static int sgxfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  string filename = strip_leading_slash(path);

  int found;
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());

  if (!found) {
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
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());
  if (!found) {
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
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());
  if (found) {
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
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());
  if (!found) {
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
int sgxfs_mkdir(const char *, mode_t) {
  cout << "sgxfs_mkdir not implemented" << endl;
  return -EINVAL;
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

void destroy(void* private_data) {
  sgx_destroy_enclave(ENCLAVE_ID);
}

static struct fuse_operations sgxfs_oper;

int main(int argc, char **argv) {
  if (initialize_enclave(&ENCLAVE_ID, "enclave.token", "enclave.signed.so") < 0) {
    cerr << "Fail to initialize enclave." << endl;
    return 1;
  }
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
  sgxfs_oper.destroy = destroy;

  return fuse_main(argc, argv, &sgxfs_oper, NULL);
}
}
