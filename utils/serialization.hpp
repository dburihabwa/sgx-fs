#ifndef __SERIALIZATION_H__
#include <map>
#include <string>
#include <vector>

void dump(const char*, const std::string &path, const size_t bytes);
void dump_map(const std::map<std::string, std::vector < std::vector < char>*>*> &files,
              const std::string &directory_path);
size_t restore(const std::string &path, char *buffer);
std::map<std::string, std::vector<std::vector< char>*>*>* restore_map(const std::string &path);

#define __SERIALIZATION_H__
#endif /* __SERIALIZATION_H__*/
