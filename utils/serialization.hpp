#ifndef __SERIALIZATION_H__
#include <map>
#include <string>
#include <vector>

#include "sgx_tseal.h"

void dump(const char*, const std::string &path, const size_t bytes);
void dump_map(const std::map<std::string, std::vector<std::vector<char>*>*> &files,
              const std::string &directory_path);
size_t restore(const std::string &path, char *buffer);
std::map<std::string, std::vector<std::vector< char>*>*>* restore_map(const std::string &path);

// SGX related functions
/**
 * Dumps the from memory to disk
 * @param files Files to dump
 * @param directory_path Path to the directory where the files are to be dumped
 */
void dump_sgx_map(const std::map<std::string, std::vector<sgx_sealed_data_t*>*> &files, const std::string &directory_path);

/**
 * Restores SGX sealed data dumped using `dump_sgx_map`
 * @param path Path to the directory where the files were dumped
 * @return Initialzed files structure
 */
std::map<std::string, std::vector<sgx_sealed_data_t*>*>* restore_sgx_map(const std::string &path);

#define __SERIALIZATION_H__
#endif /* __SERIALIZATION_H__*/
