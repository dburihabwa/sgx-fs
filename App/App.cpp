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
static map<string, bool> DIRECTORIES;

sgx_enclave_id_t ENCLAVE_ID;

static string strip_leading_slash(const string filename) {
	string stripped = filename;
	while (stripped.length() > 0 && stripped.front() == '/') {
		stripped = stripped.substr(1, string::npos);
	}
	return stripped;
}

static string strip_trailing_slash(const string filename) {
	string stripped = filename;
	while (stripped.length() > 0 && stripped.back() == '/') {
		stripped.pop_back();
	}
	return stripped;
}

static string clean_path(const string filename) {
	string trimmed = strip_leading_slash(strip_trailing_slash(filename));
	size_t position ;
	while ((position = trimmed.find("//")) != string::npos) {
		trimmed = trimmed.replace(position, 2, "/");
	}
	return trimmed;
}

static bool starts_with(const string pattern, const string path) {
    if (path.length() <= pattern.length()) {
        return false;
    }
    return path.compare(0, pattern.length(), pattern.c_str()) == 0;
}

static string get_relative_path(const string directory, const string file) {
	string directory_path = clean_path(directory);
	string file_path = clean_path(file);
	if (!starts_with(directory_path, file_path)) {
		throw runtime_error("directory and file do not start with the same substring");
	}
	return clean_path(file_path.substr(directory_path.length(), string::npos));
}

/**
 * Test if a file is located in a directory NOT in its subdirectories.
 * @param directory Path to the directory
 * @param file Path to the file
 * @return True if the file is directly located in the directory. False otherwise
 */
static bool is_in_directory(const string directory, const string file) {
	string directory_path = clean_path(directory);
	string file_path = clean_path(file);
    if (!starts_with(directory_path, file_path)) {
		return false;
	}
    string relative_file_path = get_relative_path(directory_path, file_path);
    if (relative_file_path.find("/") != string::npos) {
        return false;
    }
    return true;
}

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

static size_t compute_file_size(vector<sgx_sealed_data_t*>* data) {
  size_t size = 0;
  for (auto it = data->begin(); it != data->end(); it++) {
    size += (*it)->aes_data.payload_size;
  }
  return size;
}

void ocall_print(const char* str) {
  printf("[ocall_print] %s\n", str);
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

  if (FILES.find(filename) != FILES.end()) {
    auto entry = FILES.find(filename);
    auto data = entry->second;
    stbuf->st_size = compute_file_size(&data);
    stbuf->st_mode = S_IFREG | 0777;
    stbuf->st_nlink = 1;
    return 0;
  }
  cerr << "[ramfs_getattr] Could not find entry for " << path << endl;
  return -ENOENT;
}

static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
  string pathname = clean_path(path);
  if (FILES.find(pathname) != FILES.end()) {
	  return -ENOTDIR;
  }
  if (!pathname.empty() && DIRECTORIES.find(pathname) == DIRECTORIES.end()) {
	  return -ENOENT;
  }
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  vector<string> entries;
  for (auto it = DIRECTORIES.begin(); it != DIRECTORIES.end(); it++) {
      if (is_in_directory(pathname, it->first)) {
		  entries.push_back(get_relative_path(pathname, it->first));
      }
  }
  for (auto it = FILES.begin(); it != FILES.end(); it++) {
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
  if (FILES.find(filename) == FILES.end()) {
    cout << "ramfs_open(" << filename << "): Not found" << endl;
    return -ENOENT;
  }

  return 0;
}

static int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  string filename = clean_path(path);
  auto entry = FILES.find(filename);
  
  if (entry == FILES.end()) {
    cerr << "[ramfs_read] " << filename << ": Not found" << endl;
    return -ENOENT;
  }
  auto blocks = entry->second;
  auto block_index = size_t(floor(offset / BLOCK_SIZE));
  if (blocks.size() <= block_index) {
    return 0;
  }
  auto max_size = compute_file_size(&blocks);
  if (max_size < (offset + size)) {
    return 0;
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
  string filename = clean_path(path);
  auto entry = FILES.find(filename);
  if (entry == FILES.end()) {
    cerr << "[ramfs_write] " << filename << ": Not found" << endl;
    return -ENOENT;
  }
  vector<sgx_sealed_data_t*>& blocks = entry->second;
  auto block_index = size_t(floor(offset / BLOCK_SIZE));
  auto offset_in_block = offset % BLOCK_SIZE;
  auto payload_size = offset_in_block + size;
  if (block_index < blocks.size()) {
    sgx_sealed_data_t* block = blocks[block_index];
    auto current_payload_size = block->aes_data.payload_size;
    auto current_sealed_size = sizeof(sgx_sealed_data_t) + block->aes_data.payload_size;
    auto new_payload_size = (payload_size > current_payload_size) ? payload_size : current_payload_size;
    uint8_t* plaintext = new uint8_t[new_payload_size];
    auto new_sealed_size = sizeof(sgx_sealed_data_t) + new_payload_size;
    sgx_status_t ret;
    ramfs_decrypt(ENCLAVE_ID,
                  &ret,
                  filename.c_str(),
                  block, current_sealed_size,
                  plaintext, block->aes_data.payload_size);
    for (size_t i = 0; i < size; i++) {
      plaintext[offset_in_block + i] = data[i];
    }
    free(block);
    block = (sgx_sealed_data_t*) malloc(new_sealed_size);
    ramfs_encrypt(ENCLAVE_ID,
                  &ret,
                  filename.c_str(),
                  plaintext, new_payload_size,
                  block, new_sealed_size);
    FILES[filename][block_index] = block;
    delete [] plaintext;
    return size;
  }
  uint8_t* plaintext = new uint8_t[payload_size];
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
  string filename = clean_path(pathname);
  FILES.erase(filename);
  return 0;
}

int ramfs_create(const char *path, mode_t mode, struct fuse_file_info *) {
  string filename = clean_path(path);

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
  string filename = clean_path(path);

  auto entry = FILES.find(filename);
  if (entry == FILES.end()) {
    cerr << "ramfs_truncate(" << filename << "): Not found" << endl;
    return -ENOENT;
  }

  vector<sgx_sealed_data_t*>& blocks = entry->second;
  auto file_size = compute_file_size(&blocks);

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
int ramfs_mkdir(const char* dir_path, mode_t mode) {
    string path = clean_path(dir_path);
    if (path.length() == 0) {
        return -1;
    }
    auto existing_directory = DIRECTORIES.find(path);
    if (existing_directory != DIRECTORIES.end()) {
        return 0;
    }
    auto existing_file = FILES.find(path);
    if (existing_file != FILES.end()) {
        cerr << "A file with the name " << path << " already exists!" << endl;
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
    if (FILES.find(directory) != FILES.end()) {
        errno = ENOTDIR;
        return -1;
    }
    if (FILES.lower_bound(directory) != FILES.end()) {
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

void destroy(void* private_data) {
  //TODO Dump sealed metadata
  sgx_destroy_enclave(ENCLAVE_ID);
}

int ramfs_flush(const char* path, struct fuse_file_info* fi) {
    return 0;
}

int ramfs_release(const char* path, struct fuse_file_info *fi) {
    return 0;
}

int ramfs_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
  return 0;
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
  ramfs_oper.flush = ramfs_flush;
  ramfs_oper.release = ramfs_release;
  ramfs_oper.fsync = ramfs_fsync;

  return fuse_main(argc, argv, &ramfs_oper, NULL);
}
