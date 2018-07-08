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

    FileSystem();
    explicit FileSystem(const size_t block_size);
    explicit FileSystem(std::map<std::string, std::vector<std::vector<char>*>*>* restored_files);
    ~FileSystem();

    int create(const std::string &path);
    int unlink(const std::string &path);
    int write(const std::string &path, const char *data, size_t offset, const size_t length);
    size_t get_file_size(const std::string &path) const;
    int truncate(const std::string &path, const size_t length);
    int read_data(const std::vector <std::vector<char>*>* blocks,
                  char *buffer,
                  const size_t block_index,
                  const size_t offset,
                  const size_t size);
    int read(const std::string &path, char *data, const size_t offset, const size_t length);
    int mkdir(const std::string &directory);
    int rmdir(const std::string &directory);
    std::vector<std::string> readdir(const std::string &directory) const;
    bool is_file(const std::string &path) const;
    bool is_directory(const std::string &path) const;
    bool exists(const std::string &path) const;
    /**
     * Gives the number of entries, files and directories, at the first level of a directory
     * @param directory Directory where the entries must be counted
     * @return The number of entries in the directory
     */
    int get_number_of_entries(const std::string &directory) const;
    size_t get_block_size() const;
    void set_files();
    std::map<std::string, std::vector<std::vector<char>*>*>* get_files() const;
// Path static util functions
    /**
     * Returns a copy of filename without the leading slash
     * @param filename filename to strip
     * @return a copy of filename without the leading slash
     */
    static std::string strip_leading_slash(const std::string &filename);

    /**
     * Returns a copy of filename without the trailing slash
     * @param filename filename to strip
     * @return a copy of filename without the trailing slash
     */
    static std::string strip_trailing_slash(const std::string &filename);

    /**
     * Returns a copy of the filename without the leading and trailing slashes
     * @param filename filename to clean
     * @return a copy of the filename without the leading and trailing slashes
     */
    static std::string clean_path(const std::string &filename);

    /**
     * Checks if the path string starts with the ppatern string
     * @param prefix Prefix to look for
     * @param path Path tot check
     * @return True if path starts with prefix, False otherwise
     */
    static bool starts_with(const std::string &prefix, const std::string &path);

    /**
     * Strips the file from the base directory it is part of.
     * Throws a runtime_error if file does not start with path
     * @param directory Path to remove
     * @param path absolute path to create relative path from
     * @return Relative path
     */
    static std::string get_relative_path(const std::string &directory, const std::string &path);


    /**
     * Returns a full path to the library
     * @param path Path to interpret
     * @return Absolute path
     */
    static std::string get_absolute_path(const std::string &path);

    /**
     * Returns the containing directory of path
     * @param path Path to interpret
     * @return Path to the directory
     */
    static std::string get_directory(const std::string &path);

    /**
     * Test if a file is located in a directory NOT in its subdirectories.
     * @param directory Path to the directory
     * @param path Path to the file
     * @return True if the file is directly located in the directory. False otherwise
     */
    static bool is_in_directory(const std::string &directory, const std::string &path);

    /**
     * Splits a path on / symbol.
     * Returns a vector that needs to be deleted by the caller
     * @param path Path to the file
     * @return A vector containing the parts
     */
    static std::vector<std::string>* split_path(const std::string &path);

  private:
    size_t block_size;
    std::map<std::string, std::vector<std::vector<char>*>*>* files;
    std::map<std::string, bool>* directories;
};

#endif /*__FILESYSTEM_HPP__*/
