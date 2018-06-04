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

#include "../utils/fs.hpp"
#include "../utils/logging.h"
#include "../utils/serialization.hpp"

static const size_t BLOCK_SIZE = 4096;

static map<string, vector < vector < char>*>*> *FILES;
static map<string, bool> DIRECTORIES;

static Logger LOGGER("./ramfs.log");

void ocall_print(const char *str) {
    printf("[ocall_print] %s\n", str);
}

static size_t compute_file_size(const vector < vector < char>*>* blocks) {
    size_t size = 0;
    for (auto it = blocks->begin(); it != blocks->end(); it++) {
        size += (*it)->size();
    }
    return size;
}

static int ramfs_getattr(const char *path, struct stat *stbuf) {
    string filename = clean_path(path);

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    if (DIRECTORIES.find(filename) != DIRECTORIES.end() || string(path).compare("/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        stbuf->st_size = BLOCK_SIZE;

        return 0;
    }

    if (FILES->find(filename) != FILES->end()) {
        auto entry = FILES->find(filename);
        auto blocks = entry->second;
        stbuf->st_size = compute_file_size(blocks);
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_nlink = 1;
        return 0;
    }
    LOGGER.error("ramfs_getattr(" + filename + "): Could not find entry");
    return -ENOENT;
}

static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    string pathname = clean_path(path);
    if (FILES->find(pathname) != FILES->end()) {
        return -ENOTDIR;
    }
    if (!pathname.empty() && DIRECTORIES.find(pathname) == DIRECTORIES.end()) {
        return -ENOENT;
    }
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    vector <string> entries;
    for (auto it = DIRECTORIES.begin(); it != DIRECTORIES.end(); it++) {
        if (is_in_directory(pathname, it->first)) {
            entries.push_back(get_relative_path(pathname, it->first));
        }
    }
    for (auto it = FILES->begin(); it != FILES->end(); it++) {
        if (is_in_directory(pathname, it->first)) {
            entries.push_back(get_relative_path(pathname, it->first));
        }
    }
    for (auto it = entries.begin(); it != entries.end(); it++) {
        filler(buf, it->c_str(), NULL, 0);
    }

    return 0;
}

static int ramfs_open(const char *path, struct fuse_file_info *fi) {
    string filename = clean_path(path);
    if (FILES->find(filename) == FILES->end()) {
        LOGGER.error("ramfs_open(" + filename + "): Not found");
        return -ENOENT;
    }

    return 0;
}

static int read_data(const vector < vector < char>*>* blocks,
                     char *buffer,
                     size_t block_index,
                     size_t offset,
                     size_t size) {
  size_t read = 0;
  size_t offset_in_block = offset % BLOCK_SIZE;
  for (size_t index = block_index;
       index < blocks->size() && read < size;
       index++, offset_in_block = 0) {
    vector<char> *block = blocks->at(index);
    auto size_to_copy = size - read;
    if (size_to_copy > block->size()) {
      size_to_copy = block->size();
    }
    memcpy(buffer + read, block->data() + offset_in_block, size_to_copy);
    read += size_to_copy;
  }
  return static_cast<int>(read);
}

static int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    string filename = clean_path(path);
    string log_line_header = "ramfs_read(" + filename + \
                              ", offset=" + to_string(offset) + \
                              ", size=" + to_string(size) + ")";
    auto start = chrono::high_resolution_clock::now();
    auto entry = FILES->find(filename);
    if (entry == FILES->end()) {
          LOGGER.error(log_line_header + "): Not found");
        return -ENOENT;
    }
    auto blocks = entry->second;
    size_t block_index = offset / BLOCK_SIZE;
    if (blocks->size() <= block_index) {
        LOGGER.error(log_line_header + \
                     ") Exiting because block_index is higher than blocks");
        return 0;
    }
    int read = read_data(blocks, buf, block_index, offset, size);
    auto end = chrono::high_resolution_clock::now();
    auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);
    LOGGER.info(log_line_header + " Exiting with " + to_string(read) + " after " + to_string(elapsed.count()) + " microseconds");
    return read;
}

int ramfs_write(const char *path, const char *data, size_t size, off_t offset,
                struct fuse_file_info *) {
    string filename = clean_path(path);
    const string header = "ramfs_write(" + filename + ", offset=" + to_string(offset) + ", size=" + to_string(size) + ")";
    auto start = chrono::high_resolution_clock::now();
    auto entry = FILES->find(filename);
    if (entry == FILES->end()) {
        LOGGER.error("ramfs_write(" + filename + "): Not found");
        return -ENOENT;
    }
    auto blocks = entry->second;
    auto block_index = size_t(floor(offset / BLOCK_SIZE));
    auto offset_in_block = offset % BLOCK_SIZE;
    size_t written = 0;
    if (block_index < blocks->size()) {
        vector<char> *block = (*blocks)[block_index];
        size_t bytes_to_write = size;
        if (BLOCK_SIZE < (offset_in_block + size)) {
          bytes_to_write = BLOCK_SIZE - offset_in_block;
          block->resize(BLOCK_SIZE);
        } else {
          block->resize(offset_in_block + size);
        }
        memcpy(block->data() + offset_in_block, data, bytes_to_write);
        written += bytes_to_write;
    }
    // Create new blocks from scratch
    while (written < size) {
      size_t bytes_to_write = size - written;
      if (BLOCK_SIZE < bytes_to_write) {
        bytes_to_write = BLOCK_SIZE;
      }
      vector<char> *block = new vector<char>(bytes_to_write);
      memcpy(block->data(), data + written, bytes_to_write);
      blocks->push_back(block);
      written += bytes_to_write;
    }
    auto end = chrono::high_resolution_clock::now();
    auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);
    LOGGER.info(header + ": Exiting " + to_string(written) + " after " + to_string(elapsed.count()) + " microseconds");
    return written;
}

int ramfs_unlink(const char *pathname) {
    string filename = clean_path(pathname);
    auto entry = FILES->find(filename);
    if (entry == FILES->end()) {
        return -ENOENT;
    }
    auto blocks = entry->second;
    for (auto it = blocks->begin(); it != blocks->end(); it++) {
        auto block = (*it);
        delete block;
    }
    blocks->clear();
    delete blocks;
    FILES->erase(filename);
    return 0;
}

int ramfs_create(const char *path, mode_t mode, struct fuse_file_info *) {
    string filename = clean_path(path);
    LOGGER.info("ramfs_create(" + filename + ") Entering");

    if (FILES->find(filename) != FILES->end()) {
        LOGGER.error("ramfs_create(" + filename + "): Already exists");
        return -EEXIST;
    }

    if ((mode & S_IFREG) == 0) {
        LOGGER.error("ramfs_create(" + filename + "): Only files may be created");
        return -EINVAL;
    }
    (*FILES)[filename] = new vector < vector < char > * > ();
    LOGGER.info(
            "ramfs_create(" + filename + ") Added new empty vector at address " +
            convert_pointer_to_string((*FILES)[filename]));
    LOGGER.info("ramfs_create(" + filename + ") Exiting");
    return 0;
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
    string filename = clean_path(path);
    LOGGER.info("[ramfs_truncate]" + filename);
    auto len = static_cast<unsigned int>(length);

    auto entry = FILES->find(filename);
    if (entry == FILES->end()) {
        LOGGER.error("ramfs_truncate(" + filename + "): Not found");
        return -ENOENT;
    }

    auto blocks = entry->second;
    size_t file_size = 0;
    for (auto it = blocks->begin(); it != blocks->end(); it++) {
        file_size += (*it)->size();
    }

    LOGGER.info("[ramfs_truncate] file size = " + to_string(file_size) + ", length = " + to_string(len));

    if (file_size == len) {
        return 0;
    }

    LOGGER.info("[ramfs_truncate] POUET");


    if (file_size <= len) {
        auto length_difference = len - blocks->size();
        auto blocks_to_add = (size_t) floor(length_difference / BLOCK_SIZE);
        if (blocks_to_add > 0) {
            for (size_t i = 0; i < blocks_to_add; i++) {
                vector<char> *dummy_block = new vector<char>(BLOCK_SIZE);
                blocks->push_back(dummy_block);
            }
        }
        auto length_of_last_block = len % BLOCK_SIZE;
        if (length_of_last_block > 0) {
            vector<char> *dummy_block = new vector<char>(length_of_last_block);
            blocks->push_back(dummy_block);
        }
        LOGGER.info("[ramfs_truncate] exiting");
        return 0;
    }


    auto blocks_to_keep = static_cast<unsigned int>(int(ceil(len / BLOCK_SIZE)));
    LOGGER.info("[ramfs_truncate] Keeping " + to_string(blocks_to_keep) + " blocks");
    while (blocks_to_keep < blocks->size()) {
        delete blocks->back();
        blocks->pop_back();
    }
    //blocks.erase(blocks.begin() + blocks_to_keep, blocks.end());
    LOGGER.info("[ramfs_truncate] " + to_string(blocks->size()) + " blocks left");
    if (blocks->empty()) {
        return 0;
    }

    auto block_to_trim = blocks->back();
    auto bytes_to_keep = len % BLOCK_SIZE;
    block_to_trim->erase(block_to_trim->begin() + bytes_to_keep, block_to_trim->end());
    LOGGER.info("[ramfs_truncate] exiting");

    return 0;
}

int ramfs_mknod(const char *path, mode_t mode, dev_t dev) {
    cout << "ramfs_mknod not implemented" << endl;
    return -EINVAL;
}

int ramfs_mkdir(const char *dir_path, mode_t mode) {
    string path = clean_path(dir_path);
    if (path.length() == 0) {
        return -1;
    }
    auto existing_directory = DIRECTORIES.find(path);
    if (existing_directory != DIRECTORIES.end()) {
        return 0;
    }
    auto existing_file = FILES->find(path);
    if (existing_file != FILES->end()) {
        LOGGER.error("A file with the name " + path + " already exists!");
        return -1;
    }
    if (path[path.length() - 1] == '/') {
        path = path.substr(0, path.length() - 1);
    }
    DIRECTORIES[path] = true;
    return 0;
}

int ramfs_rmdir(const char *path) {
    string directory = clean_path(path);
    if (FILES->find(directory) != FILES->end()) {
        errno = ENOTDIR;
        return -1;
    }
    if (FILES->lower_bound(directory) != FILES->end()) {
        errno = ENOTEMPTY;
        return -1;
    }
    auto entry = DIRECTORIES.find(directory);
    if (entry == DIRECTORIES.end()) {
        errno = ENOENT;
        return -1;
    }
    DIRECTORIES.erase(entry);
    return 0;
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
  FILES = restore_map("ramfs_dump");
  for (auto it = FILES->begin(); it != FILES->end(); it++) {
    string filename = it->first;
    vector<string>* tokens = split_path(filename);
    string directory_name;
    for (size_t i = 0; i < tokens->size() - 1; i++) {
      directory_name += tokens->at(i) + "/";
      ramfs_mkdir(directory_name.c_str(), 0777);
    }
    delete tokens;
  }
  return FILES;
}

void destroy(void* unused_private_data) {
  dump_map((*FILES), "ramfs_dump");
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
