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
  printf("%s\n", str);
}

static int ramfs_getattr(const char *path, struct stat *stbuf) {
  string filename = path;
  string stripped_slash = strip_leading_slash(filename);
  int res = 0;
  memset(stbuf, 0, sizeof(struct stat));

  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

  if (filename == "/") {
    cout << "ramfs_getattr(" << filename << "): Returning attributes for /"
         << endl;
    stbuf->st_mode = S_IFDIR | 0777;
    stbuf->st_nlink = 2;

  } else {
    int found;
    sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, stripped_slash.c_str());
    if (found) {
      cout << "ramfs_getattr(" << stripped_slash << "): Returning attributes"
         << endl;
      stbuf->st_mode = S_IFREG | 0777;
      stbuf->st_nlink = 1;
      int file_size;
      sgx_status_t status = ramfs_get_size(ENCLAVE_ID, &file_size, stripped_slash.c_str());
      stbuf->st_size = file_size;

    } else {
      cout << "ramfs_getattr(" << stripped_slash << "): not found" << endl;
      res = -ENOENT;
    }
  }

  return res;
}

static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
  cout << "[entering] ramfs_readdir" << endl;
  if (strcmp(path, "/") != 0) {
    cout << "ramfs_readdir(" << path << "): Only / allowed" << endl;
    return -ENOENT;
  }
  cout << "[ramfs_readdir] adding basic entries" << endl;
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  cout << "[ramfs_readdir] added basic entries" << endl;
  
  int number_of_entries;
  sgx_status_t status = ramfs_get_number_of_entries(ENCLAVE_ID, &number_of_entries);
  char **entries = new char*[number_of_entries];
  int size;
  cout << "[ramfs_readdir] sgx in" << endl;
  status = ramfs_list_entries(ENCLAVE_ID, &size, entries);
  cout << "[ramfs_readdir] sgx out with " << size << " entries of size " << sizeof(entries[0]) << endl;

  for (int i = 0; i < size; i++) {
    cout << "[ramfs_readdir] adding " << entries[i] << " to the list" << endl;
    filler(buf, entries[i], NULL, 0);
    cout << "[ramfs_readdir] added  " << entries[i] << " to the list" << endl;
  }
  cout << "[ramfs_readdir] freeing out data structures" << endl;
  if (entries != NULL) {
    delete(entries);
  }
  cout << "[exiting] ramfs_readdir" << endl;
  return 0;
}

static int ramfs_open(const char *path, struct fuse_file_info *fi) {
  string filename = strip_leading_slash(path);  
  int found;
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());
  
  if (!found) {
    cout << "ramfs_open(" << filename << "): Not found" << endl;
    return -ENOENT;
  }

  return 0;
}

static int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  string filename = strip_leading_slash(path);

  int found;
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());
  
  if (!found) {
    cout << "ramfs_read(" << filename << "): Not found" << endl;
    return -ENOENT;
  }
  
  int read;
  status = ramfs_get(ENCLAVE_ID, &read, filename.c_str(), offset, size, buf);
  return read;
}

int ramfs_write(const char *path, const char *data, size_t size, off_t offset,
                struct fuse_file_info *) {
  string filename = strip_leading_slash(path);

  int found;
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());
  if (!found) {
    cout << "ramfs_write(" << filename << "): Not found" << endl;
    return -ENOENT;
  }
  int written;
  status = ramfs_put(ENCLAVE_ID, &written, filename.c_str(), offset, size, data);
  return written;
}

int ramfs_unlink(const char *pathname) {
  string filename = strip_leading_slash(pathname);
  int retval;
  sgx_status_t status = ramfs_delete_file(ENCLAVE_ID, &retval, filename.c_str());
  return retval;
}

int ramfs_create(const char *path, mode_t mode, struct fuse_file_info *) {
  cout << "[ramfs_create] entering" << endl;
  string filename = strip_leading_slash(path);

  int found;
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());
  if (found) {
    cout << "ramfs_create(" << filename << "): Already exists" << endl;
    return -EEXIST;
  }

  if ((mode & S_IFREG) == 0) {
    cout << "ramfs_create(" << filename << "): Only files may be created"
         << endl;
    return -EINVAL;
  }
  int retval;
  sgx_status_t stattus = ramfs_create_file(ENCLAVE_ID, &retval, filename.c_str());
  cout << "[ramfs_create] exiting" << endl;
  return retval;
}

int ramfs_fgetattr(const char *path, struct stat *stbuf,
                   struct fuse_file_info *) {
  cout << "ramfs_fgetattr(" << path << "): Delegating to ramfs_getattr" << endl;
  return ramfs_getattr(path, stbuf);
}

int ramfs_opendir(const char *path, struct fuse_file_info *) {
  cout << "ramfs_opendir(" << path << "): access granted" << endl;
  return 0;
}

int ramfs_access(const char *path, int) {
  cout << "ramfs_access(" << path << ") access granted" << endl;
  return 0;
}

int ramfs_truncate(const char *path, off_t length) {
  string filename = strip_leading_slash(path);

  int found;
  sgx_status_t status = ramfs_file_exists(ENCLAVE_ID, &found, filename.c_str());
  if (!found) {
    cout << "ramfs_truncate(" << filename << "): Not found" << endl;
    return -ENOENT;
  }

  int retval;
  sgx_status_t stattus = ramfs_trunkate(ENCLAVE_ID, &retval,  filename.c_str(), length);
  return retval;
}

int ramfs_mknod(const char *path, mode_t mode, dev_t dev) {
  cout << "ramfs_mknod not implemented" << endl;
  return -EINVAL;
}
int ramfs_mkdir(const char *, mode_t) {
  cout << "ramfs_mkdir not implemented" << endl;
  return -EINVAL;
}
int ramfs_rmdir(const char *) {
  cout << "ramfs_rmdir not implemented" << endl;
  return -EINVAL;
}
int ramfs_symlink(const char *, const char *) {
  cout << "ramfs_symlink not implemented" << endl;
  return -EINVAL;
}
int ramfs_rename(const char *, const char *) {
  cout << "ramfs_rename not implemented" << endl;
  return -EINVAL;
}
int ramfs_link(const char *, const char *) {
  cout << "ramfs_link not implemented" << endl;
  return -EINVAL;
}
int ramfs_chmod(const char *, mode_t) {
  cout << "ramfs_chmod not implemented" << endl;
  return -EINVAL;
}
int ramfs_chown(const char *, uid_t, gid_t) {
  cout << "ramfs_chown not implemented" << endl;
  return -EINVAL;
}
int ramfs_utime(const char *, struct utimbuf *) {
  cout << "ramfs_utime not implemented" << endl;
  return -EINVAL;
}
int ramfs_utimens(const char *, const struct timespec tv[2]) {
  cout << "ramfs_utimens not implemented" << endl;
  return -EINVAL;
}
int ramfs_bmap(const char *, size_t blocksize, uint64_t *idx) {
  cout << "ramfs_bmap not implemented" << endl;
  return -EINVAL;
}
int ramfs_setxattr(const char *, const char *, const char *, size_t, int) {
  cout << "ramfs_setxattr not implemented" << endl;
  return -EINVAL;
}

static struct fuse_operations ramfs_oper;

int main(int argc, char **argv) {
  if (initialize_enclave(&ENCLAVE_ID, "enclave.token", "enclave.signed.so") < 0) {
    std::cout << "Fail to initialize enclave." << std::endl;
    return 1;
  }
  ramfs_oper.getattr = ramfs_getattr;
  ramfs_oper.readdir = ramfs_readdir;
  ramfs_oper.open = ramfs_open;
  ramfs_oper.read = ramfs_read;
  ramfs_oper.mknod = ramfs_mknod;
  ramfs_oper.write = ramfs_write;
  ramfs_oper.unlink = ramfs_unlink;

  ramfs_oper.setxattr = ramfs_setxattr;
  ramfs_oper.mkdir = ramfs_mkdir;
  ramfs_oper.rmdir = ramfs_rmdir;
  ramfs_oper.symlink = ramfs_symlink;
  ramfs_oper.rename = ramfs_rename;
  ramfs_oper.link = ramfs_link;
  ramfs_oper.chmod = ramfs_chmod;
  ramfs_oper.chown = ramfs_chown;
  ramfs_oper.truncate = ramfs_truncate;
  ramfs_oper.utime = ramfs_utime;
  ramfs_oper.opendir = ramfs_opendir;
  ramfs_oper.access = ramfs_access;
  ramfs_oper.create = ramfs_create;
  ramfs_oper.fgetattr = ramfs_fgetattr;
  ramfs_oper.utimens = ramfs_utimens;
  ramfs_oper.bmap = ramfs_bmap;

  return fuse_main(argc, argv, &ramfs_oper, NULL);
}
}
