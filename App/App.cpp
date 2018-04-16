/**
 * Code directly taken and modified from Dennis Pfisterer's fuse-ramfs
 * https://github.com/pfisterer/fuse-ramfs
 */

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
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

#include "Enclave_u.h"
#include "sgx_urts.h"
#include "sgx_utils/sgx_utils.h"

static const size_t BLOCK_SIZE = 4096;

static map<string, vector<sgx_sealed_data_t*>> FILES;

static string strip_leading_slash(string filename) {
  bool starts_with_slash = false;

  if (filename.size() > 0)
    if (filename[0] == '/')
      starts_with_slash = true;

  return starts_with_slash ? filename.substr(1, string::npos) : filename;
}

sgx_enclave_id_t ENCLAVE_ID;


static void print_buffer(const uint8_t* buffer, size_t payload_size) {
  printf("[ ");
  for (size_t i = 0; i < payload_size; i++) {
    printf("%02x ", buffer[i]);
  }
  printf("]\n");
}

static void print_sealed_data(sgx_sealed_data_t* block) {
  printf("(%p) = ", block);
  cout << "{" << endl;
  cout << "\taes_data.payload_size: " << block->aes_data.payload_size << endl;
  cout << "\taes_data.payload_tag";
  printf("(%p): ", block->aes_data.payload_tag);
  print_buffer(block->aes_data.payload_tag, SGX_SEAL_TAG_SIZE);
  cout << "\taes_data.payload";
  printf("(%p): ", block->aes_data.payload);
  print_buffer(block->aes_data.payload, block->aes_data.payload_size);
  cout << "}" << endl;
}

static size_t compute_file_size(vector<sgx_sealed_data_t*> &data) {
  size_t size = 0;
  for (auto it = data.begin(); it != data.end(); it++) {
    sgx_sealed_data_t* sealed_block = (sgx_sealed_data_t*) (*it);
    size += sealed_block->aes_data.payload_size;
  }
  return size;
}

void ocall_print(const char* str) {
  printf("[ocall_print] %s\n", str);
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
    auto entry = FILES.find(stripped_slash);
    if (entry != FILES.end()) {
      vector<sgx_sealed_data_t*> data = entry->second;
      stbuf->st_size = compute_file_size(data);

      stbuf->st_mode = S_IFREG | 0777;
      stbuf->st_nlink = 1;
      
    } else {
      res = -ENOENT;
    }
  }
  return res;
}

static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
  if (strcmp(path, "/") != 0) {
    cout << "ramfs_readdir(" << path << "): Only / allowed" << endl;
    return -ENOENT;
  }
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  
  for (auto it = FILES.begin(); it != FILES.end(); it++) {
    filler(buf, it->first.c_str(), NULL, 0);
  }

  return 0;
}

static int ramfs_open(const char *path, struct fuse_file_info *fi) {
  string filename = strip_leading_slash(path);
  if (FILES.find(filename) == FILES.end()) {
    cout << "ramfs_open(" << filename << "): Not found" << endl;
    return -ENOENT;
  }

  return 0;
}

static int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  string filename = strip_leading_slash(path);
  auto entry = FILES.find(filename);
  
  if (entry == FILES.end()) {
    cerr << "[ramfs_read] " << filename << ": Not found" << endl;
    return -ENOENT;
  }
  auto blocks = entry->second;
  auto block_index = size_t(floor(offset / BLOCK_SIZE));
  if (blocks.size() <= block_index) {
    cerr << "[ramfs_read] " << filename << ": offset too large" << endl;
    return -ENOENT;
  }
  auto max_size = blocks.size() * BLOCK_SIZE;
  if (max_size < (offset + size)) {
    cerr << "[ramfs_read] " << filename << ": offset + size too large" << endl;
    return -ENOENT;
  }
  sgx_sealed_data_t* block = entry->second[block_index];
 
  auto payload_size = block->aes_data.payload_size;
  auto sealed_size = sizeof(sgx_sealed_data_t) + payload_size;


  sgx_status_t read;
  ramfs_decrypt(ENCLAVE_ID, &read,
                filename.c_str(),
                block, sealed_size,
                (uint8_t*) buf, payload_size);
  switch (read) {
    case SGX_ERROR_INVALID_PARAMETER:
      cerr << "[ramfs_read] Invalid parameter" << endl;
      break;
    case SGX_ERROR_INVALID_CPUSVN:
      cerr << "[ramfs_read] The CPUSVN in the sealed data blob is beyond the CPUSVN value of the platform." << endl;
      break;
    case SGX_ERROR_INVALID_ISVSVN:
      cerr << "[ramfs_read] The ISVSVN in the sealed data blob is greater than the ISVSVN value of the enclave." << endl;
      break;

    case SGX_ERROR_MAC_MISMATCH:
      cerr << "[ramfs_read] The tag verification failed during unsealing. The error may be caused by a platform update, software update, or sealed data blob corruption. This error is also reported if other corruption of the sealed data structure is detected." << endl;
      break;

    case SGX_ERROR_OUT_OF_MEMORY:
      cerr << "[ramfs_read] The enclave is out of memory." << endl;
      break;

    case SGX_ERROR_UNEXPECTED:
      cerr << "[ramfs_read] Indicates a cryptography library failure." << endl;
      break;
    default:
      break;
  }
  return size;
}

int ramfs_write(const char *path, const char *data, size_t size, off_t offset,
                struct fuse_file_info *) {
  string filename = strip_leading_slash(path);
  auto entry = FILES.find(filename);
  if (entry == FILES.end()) {
    cerr << "[ramfs_write] " << filename << ": Not found" << endl;
    return -ENOENT;
  }
  vector<sgx_sealed_data_t*>& blocks = entry->second;
  auto block_index = size_t(floor(offset / BLOCK_SIZE));
  auto offset_in_block = offset % BLOCK_SIZE;
  auto payload_size = offset_in_block + size;
  uint8_t* plaintext = new uint8_t[payload_size];
  cout << "[ramfs_write] File contains " << blocks.size() << " blocks" << endl;
  cout << "[ramfs_write] About to write to block " << block_index << endl;
  if (block_index < blocks.size()) {
    sgx_sealed_data_t* block = blocks[block_index];
    auto current_sealed_size = sizeof(sgx_sealed_data_t) + block->aes_data.payload_size;
    auto new_sealed_size = sizeof(sgx_sealed_data_t) + block->aes_data.payload_size;
    sgx_status_t ret;
    ramfs_decrypt(ENCLAVE_ID,
                  &ret,
                  filename.c_str(),
                  block, current_sealed_size,
                  plaintext, block->aes_data.payload_size);
    for (size_t i = 0; i < size; i++) {
      plaintext[offset_in_block + i] = data[i];
    }
    cout << "YUP...";
    free(block);
    cout << "YUP" << endl;
    block = (sgx_sealed_data_t*) malloc(new_sealed_size);
    ramfs_encrypt(ENCLAVE_ID,
                  &ret,
                  filename.c_str(),
                  plaintext, payload_size,
                  block, new_sealed_size);
    FILES[filename][block_index] = block;
    delete [] plaintext;
    return size;
  }
  auto sealed_size = sizeof(sgx_sealed_data_t) + payload_size;
  auto block = (sgx_sealed_data_t*) malloc(sealed_size);

  for (size_t i = 0; i < size; i++) {
    plaintext[offset_in_block + i] = data[i];
  }
  sgx_status_t ret;
  sgx_status_t status = ramfs_encrypt(ENCLAVE_ID,
                                      &ret,
                                      filename.c_str(),
                                      plaintext, payload_size,
                                      block, sealed_size);
  FILES[filename].push_back(block);
  delete [] plaintext;
  return size;
}

int ramfs_unlink(const char *pathname) {
  string filename = strip_leading_slash(pathname);
  FILES.erase(filename);
  return 0;
}

int ramfs_create(const char *path, mode_t mode, struct fuse_file_info *) {
  string filename = strip_leading_slash(path);

  if (FILES.find(filename) != FILES.end()) {
    cerr << "ramfs_create(" << filename << "): Already exists" << endl;
    return -EEXIST;
  }

  if ((mode & S_IFREG) == 0) {
    cerr << "ramfs_create(" << filename << "): Only files may be created"
         << endl;
    return -EINVAL;
  }
  FILES[filename] = vector<sgx_sealed_data_t*>();
  cout << "Files in the system:" << endl;
  for (auto it = FILES.begin(); it != FILES.end(); it++) {
    cout << "  * " << it->first << endl;
  }

  return 0;
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
  cout << "[ramfs_truncate] entering" << endl;
  string filename = strip_leading_slash(path);

  auto entry = FILES.find(filename);
  if (entry == FILES.end()) {
    cerr << "ramfs_truncate(" << filename << "): Not found" << endl;
    return -ENOENT;
  }

  vector<sgx_sealed_data_t*>& blocks = entry->second;
  auto file_size = compute_file_size(blocks);

  cout << "[ramfs_truncate] file size = " << file_size << ", length = " << length << endl;

  if (file_size == length) {
    return 0;
  }

  cout << "[ramfs_truncate] POUET" << endl;


  if (file_size <= length) {
    auto length_difference = length - blocks.size();
    auto blocks_to_add = (size_t) floor(length_difference / BLOCK_SIZE);
    if (blocks_to_add > 0) {
      uint8_t* dummy_text = new uint8_t[BLOCK_SIZE];
      size_t sealed_size = sizeof(sgx_sealed_data_t) + BLOCK_SIZE;
      sgx_sealed_data_t* dummy_block = (sgx_sealed_data_t*) malloc(sealed_size);
      sgx_status_t ret;
      ramfs_encrypt(ENCLAVE_ID,
                    &ret,
                    filename.c_str(),
                    dummy_text, BLOCK_SIZE,
                    dummy_block, sealed_size);
      for (size_t i = 0; i < blocks_to_add; i++) {
        sgx_sealed_data_t* block_copy = (sgx_sealed_data_t*) malloc(sealed_size);
        memcpy(block_copy, dummy_block, sealed_size);
        blocks.push_back(block_copy);
      }
      free(dummy_block);
      delete [] dummy_text;
    }
    auto length_of_last_block = length % BLOCK_SIZE;
    if (length_of_last_block > 0) {
      uint8_t* dummy_text = new uint8_t[length_of_last_block];
      size_t sealed_size = sizeof(sgx_sealed_data_t) + length_of_last_block;
      sgx_sealed_data_t* dummy_block = (sgx_sealed_data_t*) malloc(sealed_size);
      sgx_status_t ret;
      ramfs_encrypt(ENCLAVE_ID,
                    &ret,
                    filename.c_str(),
                    dummy_text, BLOCK_SIZE,
                    dummy_block, sealed_size);
      blocks.push_back(dummy_block);
      delete [] dummy_text;
    }

    cout << "[ramfs_truncate] exiting" << endl;
    return 0;
  }


  auto blocks_to_keep = int(ceil(length / BLOCK_SIZE));
  cout << "[ramfs_truncate] Keeping " << blocks_to_keep << " blocks" << endl;
  while (blocks_to_keep < blocks.size()) {
    free(blocks.back());
    blocks.pop_back();
  }
  //blocks.erase(blocks.begin() + blocks_to_keep, blocks.end());
  cout << "[ramfs_truncate] " << blocks_to_keep << " blocks left" << endl;
  if (blocks.empty()) {
    return 0;
  }

  auto block_to_trim = blocks.back();
  auto bytes_to_keep = length % BLOCK_SIZE;
  auto payload_size = block_to_trim->aes_data.payload_size;
  auto sealed_size = sizeof(sgx_sealed_data_t) + payload_size;
  uint8_t* plaintext = new uint8_t[payload_size];
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
  delete [] plaintext;
  cout << "[ramfs_truncate] exiting" << endl;

  return 0;
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

void destroy(void* private_data) {
  //TODO Dump sealed metadata
  sgx_destroy_enclave(ENCLAVE_ID);
}

static struct fuse_operations ramfs_oper;

int main(int argc, char **argv) {
  if (initialize_enclave(&ENCLAVE_ID, "enclave.token", "enclave.signed.so") < 0) {
    cerr << "Fail to initialize enclave." << endl;
    exit(1);
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
  ramfs_oper.destroy = destroy;

  return fuse_main(argc, argv, &ramfs_oper, NULL);
}
