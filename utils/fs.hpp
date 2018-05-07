//
// Created by dorian on 22/04/18.
//

#ifndef FUSEGX_FS_H
#define FUSEGX_FS_H

#include <string>
#include <stdexcept>

using namespace std;

/**
 * Returns a copy of filename without the leading slash
 * @param filename filename to strip
 * @return a copy of filename without the leading slash
 */
string strip_leading_slash(const string &filename);

/**
 * Returns a copy of filename without the trailing slash
 * @param filename filename to strip
 * @return a copy of filename without the trailing slash
 */
string strip_trailing_slash(const string &filename);

/**
 * Returns a copy of the filename without the leading and trailing slashes
 * @param filename filename to clean
 * @return a copy of the filename without the leading and trailing slashes
 */
string clean_path(const string &filename);

/**
 * Checks if the path string starts with the ppatern string
 * @param prefix Prefix to look for
 * @param path Path tot check
 * @return True if path starts with prefix, False otherwise
 */
bool starts_with(const string &prefix, const string &path);

/**
 * Strips the file from the base directory it is part of.
 * Throws a runtime_error if file does not start with path
 * @param directory Path to remove
 * @param path absolute path to create relative path from
 * @return Relative path
 */
string get_relative_path(const string &directory, const string &path);

/**
 * Test if a file is located in a directory NOT in its subdirectories.
 * @param directory Path to the directory
 * @param path Path to the file
 * @return True if the file is directly located in the directory. False otherwise
 */
bool is_in_directory(const string &directory, const string &path);

#endif //FUSEGX_FS_H
