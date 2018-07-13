/**
 * Code directly taken and modified from Dennis Pfisterer's fuse-ramfs
 * https://github.com/pfisterer/fuse-ramfs
 */


#define FUSE_USE_VERSION 26

#include <fcntl.h>
#include <fuse.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cmath>
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
#include "../utils/logging.h"
#include "../utils/serialization.hpp"

using namespace std;

static char* BINARY_NAME;

static const size_t BLOCK_SIZE = 4096;

static map<string, vector < sgx_sealed_data_t * >*>* FILES;
static map<string, bool> DIRECTORIES;

sgx_enclave_id_t ENCLAVE_ID;

static Logger LOGGER("./sgx-ramfs.log");


static size_t compute_file_size(vector<sgx_sealed_data_t *>* data) {
    size_t size = 0;
    size_t counter = 0;
    for (auto it = data->begin(); it != data->end(); it++) {
        sgx_sealed_data_t* block = (*it);
        size += block->aes_data.payload_size;
    }
    return size;
}

void ocall_print(const char *str) {
    printf("[ocall_print] %s\n", str);
}

int ramfs_getattr(const char *path, struct stat *stbuf) {
    string filename = clean_path(path);

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    if (DIRECTORIES.find(filename) != DIRECTORIES.end() ||
        string(path).compare("/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        stbuf->st_size = BLOCK_SIZE;

        return 0;
    }
    auto entry = FILES->find(filename);
    if (entry != FILES->end()) {
        auto blocks = entry->second;
        stbuf->st_size = compute_file_size(blocks);
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_nlink = 1;
        return 0;
    }
    //LOGGER.error("ramfs_getattr(" + filename + "): Could not find entry");
    return -ENOENT;
}

int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
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

int ramfs_open(const char *path, struct fuse_file_info *fi) {
    string filename = clean_path(path);
    if (FILES->find(filename) == FILES->end()) {
        //LOGGER.error("ramfs_open(" + filename + "): Not found");
        return -ENOENT;
    }

    return 0;
}

static sgx_status_t decrypt_block(sgx_sealed_data_t *sealed,
                                  char *decrypted,
                                  size_t offset,
                                  size_t size) {
  auto payload_size = sealed->aes_data.payload_size;
  auto sealed_size = sizeof(sgx_sealed_data_t) + payload_size;
  uint8_t* buffer = new uint8_t[payload_size];

  sgx_status_t read;
  ramfs_decrypt(ENCLAVE_ID, &read,
                "",
                sealed, sealed_size,
                buffer, payload_size);
  switch (read) {
      case SGX_ERROR_INVALID_PARAMETER:
          //LOGGER.error("[ramfs_read] Invalid parameter");
          break;
      case SGX_ERROR_INVALID_CPUSVN:
          //LOGGER.error("[ramfs_read] The CPUSVN in the sealed data blob is beyond the CPUSVN value of the platform.");
          break;
      case SGX_ERROR_INVALID_ISVSVN:
          //LOGGER.error( "[ramfs_read] The ISVSVN in the sealed data blob is greater than the ISVSVN value of the enclave.");
          break;
      case SGX_ERROR_MAC_MISMATCH:
          //LOGGER.error( "[ramfs_read] The tag verification failed during unsealing. The error may be caused by a platform update, software update, or sealed data blob corruption. This error is also reported if other corruption of the sealed data structure is detected.");
          break;
      case SGX_ERROR_OUT_OF_MEMORY:
          //LOGGER.error("[ramfs_read] The enclave is out of memory.");
          break;
      case SGX_ERROR_UNEXPECTED:
          //LOGGER.error("[ramfs_read] Indicates a cryptography library failure.");
          break;
      default:
          break;
  }
  auto size_to_copy = size;
  if ((size_to_copy + offset) > payload_size) {
    size_to_copy = payload_size - offset;
  }
  memcpy(decrypted + offset, buffer, size_to_copy);
  delete [] buffer;
  return (sgx_status_t) size_to_copy;
}

static int read_data(vector<sgx_sealed_data_t*> *blocks,
                     char *buffer,
                     size_t block_index,
                     size_t offset,
                     size_t size) {
  int read = 0;
  size_t offset_in_block = offset % BLOCK_SIZE;
  for (size_t index = block_index;
       index < blocks->size() && read < (int) size;
       index++, offset_in_block = 0) {
    sgx_sealed_data_t *block = blocks->at(index);
    read += decrypt_block(block, buffer + read, offset_in_block, BLOCK_SIZE);
  }
  return read;
}

int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    string filename = clean_path(path);
    string log_line_header = "ramfs_read(" + filename + \
                              ", offset=" + to_string(offset) + \
                              ", size=" + to_string(size) + ")";
    auto entry = FILES->find(filename);
    if (entry == FILES->end()) {
        //LOGGER.error(log_line_header + "): Not found");
        return -ENOENT;
    }
    auto blocks = entry->second;
    auto block_index = size_t(floor(offset / BLOCK_SIZE));
    if (blocks->size() <= block_index) {
        //LOGGER.error(log_line_header + \
                     ") Exiting because block_index is higher than blocks");
        return 0;
    }
    auto read = read_data(blocks, buf, block_index, offset, size);
    return read;
}

int ramfs_write(const char *path, const char *data, size_t size, off_t offset,
                struct fuse_file_info *) {
    string filename = clean_path(path);
    auto entry = FILES->find(filename);
    if (entry == FILES->end()) {
        //LOGGER.error("ramfs_write(" + filename + "): Not found");
        return -ENOENT;
    }
    auto blocks = entry->second;
    //LOGGER.info("ramfs_write(" + filename + ") Modifying block vector " + convert_pointer_to_string(blocks));
    auto block_index = size_t(floor(offset / BLOCK_SIZE));
    auto offset_in_block = offset % BLOCK_SIZE;
    size_t payload_size = offset_in_block + size;
    auto max_block = blocks->size() + 1;
    //LOGGER.info("ramfs_write(" + filename + ") Writing to block " + to_string(block_index) + " (payload_size = " + to_string(payload_size) + ")");
    if (block_index < blocks->size()) {
        sgx_sealed_data_t *block = blocks->data()[block_index];
        //LOGGER.info("ramfs_write(" + filename + ") Modifying block " + convert_pointer_to_string(block));
        auto current_payload_size = block->aes_data.payload_size;
        auto current_sealed_size = sizeof(sgx_sealed_data_t) + block->aes_data.payload_size;
        auto new_payload_size = (payload_size > current_payload_size) ? payload_size : current_payload_size;
        uint8_t *plaintext = new uint8_t[new_payload_size];
        auto new_sealed_size = sizeof(sgx_sealed_data_t) + new_payload_size;
        sgx_status_t ret;
        ramfs_decrypt(ENCLAVE_ID,
                      &ret,
                      filename.c_str(),
                      block, current_sealed_size,
                      plaintext, block->aes_data.payload_size);
        memcpy(plaintext + offset_in_block, data, payload_size);
        free(block);
        block = (sgx_sealed_data_t *) malloc(new_sealed_size);
        ramfs_encrypt(ENCLAVE_ID,
                      &ret,
                      filename.c_str(),
                      plaintext, new_payload_size,
                      block, new_sealed_size);
        (*(*FILES)[filename])[block_index] = block;
        delete[] plaintext;
        //LOGGER.info("ramfs_write(" + filename + "): Exiting " + to_string(size));
        return size;
    }
    uint8_t *plaintext = new uint8_t[payload_size];
    size_t sealed_size = sizeof(sgx_sealed_data_t) + payload_size;
    auto *block = (sgx_sealed_data_t *) calloc(sealed_size, sizeof(char));
    //LOGGER.info("ramfs_write(" + filename + ") Writing to block at " + convert_pointer_to_string(block));
    memcpy(plaintext + offset_in_block, data, payload_size);
    sgx_status_t ret;
    sgx_status_t status = ramfs_encrypt(ENCLAVE_ID,
                                        &ret,
                                        filename.c_str(),
                                        plaintext, payload_size,
                                        block, sealed_size);
    //LOGGER.info("ramfs_write(" + filename + ") Encrypted payload size in the block " + to_string(block->aes_data.payload_size));
    blocks->push_back(block);
    delete[] plaintext;
    //LOGGER.info("ramfs_write(" + filename + "): Exiting " + to_string(payload_size));
    return payload_size;
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
        free(block);
    }
    blocks->clear();
    delete blocks;
    FILES->erase(filename);
    return 0;
}

int ramfs_create(const char *path, mode_t mode, struct fuse_file_info *) {
    string filename = clean_path(path);
    //LOGGER.info("ramfs_create(" + filename + ") Entering");

    if (FILES->find(filename) != FILES->end()) {
        //LOGGER.error("ramfs_create(" + filename + "): Already exists");
        return -EEXIST;
    }

    if ((mode & S_IFREG) == 0) {
        //LOGGER.error("ramfs_create(" + filename + "): Only files may be created");
        return -EINVAL;
    }
    (*FILES)[filename] = new vector<sgx_sealed_data_t *>();
    //LOGGER.info("ramfs_create(" + filename + ") Added new empty vector at address " + convert_pointer_to_string((*FILES)[filename]));
    //LOGGER.info("ramfs_create(" + filename + ") Exiting");
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
    //LOGGER.info("[ramfs_truncate]" + filename);
    auto len = static_cast<unsigned int>(length);

    auto entry = FILES->find(filename);
    if (entry == FILES->end()) {
        //LOGGER.error("ramfs_truncate(" + filename + "): Not found");
        return -ENOENT;
    }

    auto blocks = entry->second;
    auto file_size = compute_file_size(blocks);

    //LOGGER.info("[ramfs_truncate] file size = " + to_string(file_size) + ", length = " + to_string(len));

    if (file_size == len) {
        return 0;
    }

    //LOGGER.info("[ramfs_truncate] POUET");


    if (file_size <= len) {
        auto length_difference = len - blocks->size();
        auto blocks_to_add = (size_t) floor(length_difference / BLOCK_SIZE);
        if (blocks_to_add > 0) {
            uint8_t *dummy_text = new uint8_t[BLOCK_SIZE];
            size_t sealed_size = sizeof(sgx_sealed_data_t) + BLOCK_SIZE;
            sgx_sealed_data_t *dummy_block = (sgx_sealed_data_t *) malloc(sealed_size);
            sgx_status_t ret;
            ramfs_encrypt(ENCLAVE_ID,
                          &ret,
                          filename.c_str(),
                          dummy_text, BLOCK_SIZE,
                          dummy_block, sealed_size);
            for (size_t i = 0; i < blocks_to_add; i++) {
                sgx_sealed_data_t *block_copy = (sgx_sealed_data_t *) malloc(sealed_size);
                memcpy(block_copy, dummy_block, sealed_size);
                blocks->push_back(block_copy);
            }
            free(dummy_block);
            delete[] dummy_text;
        }
        auto length_of_last_block = len % BLOCK_SIZE;
        if (length_of_last_block > 0) {
            uint8_t *dummy_text = new uint8_t[length_of_last_block];
            size_t sealed_size = sizeof(sgx_sealed_data_t) + length_of_last_block;
            sgx_sealed_data_t *dummy_block = (sgx_sealed_data_t *) malloc(sealed_size);
            sgx_status_t ret;
            ramfs_encrypt(ENCLAVE_ID,
                          &ret,
                          filename.c_str(),
                          dummy_text, BLOCK_SIZE,
                          dummy_block, sealed_size);
            blocks->push_back(dummy_block);
            delete[] dummy_text;
        }

        //LOGGER.info("[ramfs_truncate] exiting");
        return 0;
    }


    auto blocks_to_keep = static_cast<unsigned int>(int(ceil(len / BLOCK_SIZE)));
    //LOGGER.info("[ramfs_truncate] Keeping " + to_string(blocks_to_keep) + " blocks");
    while (blocks_to_keep < blocks->size()) {
        free(blocks->back());
        blocks->pop_back();
    }
    //blocks.erase(blocks.begin() + blocks_to_keep, blocks.end());
    //LOGGER.info("[ramfs_truncate] " + to_string(blocks->size()) + " blocks left");
    if (blocks->empty()) {
        return 0;
    }

    auto block_to_trim = blocks->back();
    auto bytes_to_keep = len % BLOCK_SIZE;
    auto payload_size = block_to_trim->aes_data.payload_size;
    auto sealed_size = sizeof(sgx_sealed_data_t) + payload_size;
    uint8_t *plaintext = new uint8_t[payload_size];
    sgx_status_t ret;
    ramfs_decrypt(ENCLAVE_ID,
                  &ret,
                  filename.c_str(),
                  block_to_trim, sealed_size,
                  plaintext, payload_size);
    ramfs_encrypt(ENCLAVE_ID,
                  &ret,
                  filename.c_str(),
                  plaintext, bytes_to_keep,
                  block_to_trim, sizeof(sgx_sealed_data_t) + bytes_to_keep);
    delete[] plaintext;
    //LOGGER.info("[ramfs_truncate] exiting");

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
        //LOGGER.error("A file with the name " + path + " already exists!");
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
  Logger init_log("sgx-ramfs-mount.log");
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
  FILES = restore_sgx_map("sgx_ramfs_dump");
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
  chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
  init_log.info("Mounted in " + to_string(duration) + " nanoseconds");
  return FILES;
}

static void dump_fs(const string &path) {
  // TODO(dburihabwa) Support dumping of files and directories in a hierarchy
  for (auto it = FILES->begin(); it != FILES->end(); it++) {
    auto pathname = it->first;
    auto blocks = it->second;
    auto sealed_size = compute_file_size(blocks) + (blocks->size() * sizeof(sgx_sealed_data_t));
    auto dump_pathname = path + "/" + pathname;
    char* sealed_data = new char[sealed_size];
    size_t offset = 0;
    for (auto b = blocks->begin(); b != blocks->end(); b++) {
      sgx_sealed_data_t* block = (*b);
      auto block_size = sizeof(sgx_sealed_data_t) + block->aes_data.payload_size;
      memcpy(sealed_data + offset, reinterpret_cast<char*>(block), block_size);
      offset += block_size;
    }
    dump(reinterpret_cast<char*>((sealed_data)), dump_pathname, sealed_size);
    free(sealed_data);
  }
}

void destroy(void* unused_private_data) {
  Logger init_log("sgx-ramfs-mount.log");
  chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();
  dump_fs("sgx_ramfs_dump");
  sgx_destroy_enclave(ENCLAVE_ID);
  chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
  init_log.info("Unmounted in " + to_string(duration) + " nanoseconds");
}

static struct fuse_operations sgx_ramfs_oper;
int main(int argc, char **argv) {
    BINARY_NAME = argv[0];
    sgx_ramfs_oper.getattr = ramfs_getattr;
    sgx_ramfs_oper.readdir = ramfs_readdir;
    sgx_ramfs_oper.open = ramfs_open;
    sgx_ramfs_oper.read = ramfs_read;
    sgx_ramfs_oper.mknod = ramfs_mknod;
    sgx_ramfs_oper.write = ramfs_write;
    sgx_ramfs_oper.unlink = ramfs_unlink;

    sgx_ramfs_oper.setxattr = ramfs_setxattr;
    sgx_ramfs_oper.mkdir = ramfs_mkdir;
    sgx_ramfs_oper.rmdir = ramfs_rmdir;
    sgx_ramfs_oper.symlink = ramfs_symlink;
    sgx_ramfs_oper.rename = ramfs_rename;
    sgx_ramfs_oper.link = ramfs_link;
    sgx_ramfs_oper.chmod = ramfs_chmod;
    sgx_ramfs_oper.chown = ramfs_chown;
    sgx_ramfs_oper.truncate = ramfs_truncate;
    sgx_ramfs_oper.utime = ramfs_utime;
    sgx_ramfs_oper.opendir = ramfs_opendir;
    sgx_ramfs_oper.access = ramfs_access;
    sgx_ramfs_oper.create = ramfs_create;
    sgx_ramfs_oper.fgetattr = ramfs_fgetattr;
    sgx_ramfs_oper.utimens = ramfs_utimens;
    sgx_ramfs_oper.bmap = ramfs_bmap;
    sgx_ramfs_oper.flush = ramfs_flush;
    sgx_ramfs_oper.release = ramfs_release;
    sgx_ramfs_oper.fsync = ramfs_fsync;

    sgx_ramfs_oper.init = init;
    sgx_ramfs_oper.destroy = destroy;

    return fuse_main(argc, argv, &sgx_ramfs_oper, NULL);
}
