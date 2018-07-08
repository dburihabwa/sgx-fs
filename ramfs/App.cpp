/**
 * Code directly taken and modified from Dennis Pfisterer's fuse-ramfs
 * https://github.com/pfisterer/fuse-ramfs
 */

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>


#define FUSE_USE_VERSION 26

#include <fcntl.h>
#include <fuse.h>

using namespace std;

#include "../utils/filesystem.hpp"
#include "../utils/fs.hpp"
#include "../utils/logging.h"
#include "../utils/serialization.hpp"

static FileSystem* FILE_SYSTEM;

static Logger LOGGER("./ramfs.log");

static int ramfs_getattr(const char *path, struct stat *stbuf) {
    string filename = FileSystem::clean_path(path);

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    if (FILE_SYSTEM->is_directory(filename)) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        stbuf->st_size = FILE_SYSTEM->get_block_size();
        return 0;
    }

    if (FILE_SYSTEM->is_file(filename)) {
        stbuf->st_size = FILE_SYSTEM->get_file_size(filename);
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_nlink = 1;
        return 0;
    }
    return -ENOENT;
}

static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    string pathname = FileSystem::clean_path(path);
    if (FILE_SYSTEM->is_file(pathname)) {
        return -ENOTDIR;
    }
    if (!FILE_SYSTEM->is_directory(pathname)) {
        return -ENOENT;
    }
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    vector <string> entries = FILE_SYSTEM->readdir(pathname);
    for (auto it = entries.begin(); it != entries.end(); it++) {
        filler(buf, it->c_str(), NULL, 0);
    }
    return 0;
}

static int ramfs_open(const char *path, struct fuse_file_info *fi) {
    string filename = FileSystem::clean_path(path);
    if (!FILE_SYSTEM->is_file(filename)) {
        return -ENOENT;
    }
    return 0;
}

static int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    string filename = FileSystem::clean_path(path);
    string log_line_header = "ramfs_read(" + filename + \
                              ", offset=" + to_string(offset) + \
                              ", size=" + to_string(size) + ")";
    auto start = chrono::high_resolution_clock::now();
    if (!FILE_SYSTEM->is_file(filename)) {
      return -ENOENT;
    }
    int read = FILE_SYSTEM->read(filename, buf, offset, size);
    auto end = chrono::high_resolution_clock::now();
    auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);
    //LOGGER.info(log_line_header + " Exiting with " + to_string(read) + " after " + to_string(elapsed.count()) + " microseconds");
    return read;
}

int ramfs_write(const char *path, const char *data, size_t size, off_t offset,
                struct fuse_file_info *) {
    string filename = FileSystem::clean_path(path);
    const string header = "ramfs_write(" + filename + ", offset=" + to_string(offset) + ", size=" + to_string(size) + ")";
    auto start = chrono::high_resolution_clock::now();
    size_t written = FILE_SYSTEM->write(filename, data, offset, size);
    auto end = chrono::high_resolution_clock::now();
    auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);
    //LOGGER.info(header + ": Exiting " + to_string(written) + " after " + to_string(elapsed.count()) + " microseconds");
    return written;
}

int ramfs_unlink(const char *pathname) {
    return FILE_SYSTEM->unlink(pathname);
}

int ramfs_create(const char *path, mode_t mode, struct fuse_file_info *) {
    return FILE_SYSTEM->create(path);
}

int ramfs_fgetattr(const char *path, struct stat *stbuf,
                   struct fuse_file_info *) {
    return ramfs_getattr(path, stbuf);
}

int ramfs_opendir(const char *path, struct fuse_file_info *) {
    return 0;
}

int ramfs_access(const char *path, int) {
    return 0;
}

int ramfs_truncate(const char *path, off_t length) {
  return FILE_SYSTEM->truncate(path, length);
}

int ramfs_mknod(const char *path, mode_t mode, dev_t dev) {
    cout << "ramfs_mknod not implemented" << endl;
    return -EINVAL;
}

int ramfs_mkdir(const char *dir_path, mode_t mode) {
  return FILE_SYSTEM->mkdir(dir_path);
}

int ramfs_rmdir(const char *path) {
  return FILE_SYSTEM->rmdir(path);
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
    return 0;
}

int ramfs_bmap(const char *, size_t blocksize, uint64_t *idx) {
    cout << "ramfs_bmap not implemented" << endl;
    return -EINVAL;
}

int ramfs_setxattr(const char *, const char *, const char *, size_t, int) {
    cout << "ramfs_setxattr not implemented" << endl;
    return -EINVAL;
}

int ramfs_flush(const char *path, struct fuse_file_info *fi) {
    return 0;
}

int ramfs_release(const char *path, struct fuse_file_info *fi) {
    return 0;
}

int ramfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
    return 0;
}

void* init(struct fuse_conn_info *conn) {
  Logger init_log("ramfs-mount.log");
  chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();
  FILE_SYSTEM = new FileSystem(restore_map("ramfs_dump"));
  chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
  init_log.info("Mounted in " + to_string(duration) + " nanoseconds");
  return FILE_SYSTEM;
}

void destroy(void* unused_private_data) {
  Logger init_log("ramfs-mount.log");
  chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();
  dump_map(FILE_SYSTEM->get_files(), "ramfs_dump");
  delete FILE_SYSTEM;
  chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
  init_log.info("Unmounted in " + to_string(duration) + " nanoseconds");
}

static struct fuse_operations ramfs_oper;

int main(int argc, char **argv) {
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
    ramfs_oper.flush = ramfs_flush;
    ramfs_oper.release = ramfs_release;
    ramfs_oper.fsync = ramfs_fsync;

    ramfs_oper.init = init;
    ramfs_oper.destroy = destroy;

    return fuse_main(argc, argv, &ramfs_oper, NULL);
}
