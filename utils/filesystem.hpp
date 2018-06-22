#ifndef __FILESYSTEM_HPP__
#define __FILESYSTEM_HPP__

#include <map>
#include <string>
#include <vector>


/**
 * An in-memory file system
 */
class FileSystem {
    public:
        static const size_t DEFAULT_BLOCK_SIZE = 4096;

        explicit FileSystem(const size_t block_size);
        explicit FileSystem(std::map<std::string, std::vector<std::vector<char>*>*>* restored_files);
        ~FileSystem();

        int create(const std::string &path);
        int unlink(const std::string &path);
        int write(const std::string &path, char *data, size_t offset, const size_t length);
        int read_data(const std::vector <std::vector<char>*>* blocks,
                      char *buffer,
                      const size_t block_index,
                      const size_t offset,
                      const size_t size);
        int read(const std::string &path, char *data, const size_t offset, const size_t length);
        int mkdir(const std::string &directory);
        int rmdir(const std::string &directory);

        size_t get_block_size() const;
        void set_files();

    private:
        size_t block_size;
        std::map<std::string, std::vector<std::vector<char>*>*>* files;
        std::map<std::string, bool>* directories;
};

#endif /*__FILESYSTEM_HPP__*/
