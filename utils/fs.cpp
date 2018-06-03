#include <climits>
#include <string>
#include <vector>

#include "fs.hpp"

string strip_leading_slash(const string &filename) {
    string stripped = filename;
    while (stripped.length() > 0 && stripped.front() == '/') {
        stripped = stripped.substr(1, string::npos);
    }
    return stripped;
}

string strip_trailing_slash(const string &filename) {
    string stripped = filename;
    while (stripped.length() > 0 && stripped.back() == '/') {
        stripped.pop_back();
    }
    return stripped;
}

string clean_path(const string &filename) {
    string trimmed = strip_leading_slash(strip_trailing_slash(filename));
    size_t position;
    while ((position = trimmed.find("//")) != string::npos) {
        trimmed = trimmed.replace(position, 2, "/");
    }
    return trimmed;
}

bool starts_with(const string &pattern, const string &path) {
    if (path.length() <= pattern.length()) {
        return false;
    }
    return path.compare(0, pattern.length(), pattern.c_str()) == 0;
}

string get_relative_path(const string &directory, const string &file) {
    string directory_path = clean_path(directory);
    string file_path = clean_path(file);
    if (!starts_with(directory_path, file_path)) {
        throw runtime_error("directory and file do not start with the same substring");
    }
    return clean_path(file_path.substr(directory_path.length(), string::npos));
}

/**
 * Returns a full path to the library
 * @param path Path to interpret
 * @return Absolute path
 */
string get_absolute_path(const string &path) {
  char absolute_path[PATH_MAX];
  realpath(path.c_str(), absolute_path);
  return string(absolute_path);
}

string get_directory(const string &path) {
  string absolute_path = get_absolute_path(path);
  size_t pos = absolute_path.rfind("/");
  if (pos == string::npos) {
    return "/";
  }
  return absolute_path.substr(0, pos);
}

/**
 * Test if a file is located in a directory NOT in its subdirectories.
 * @param directory Path to the directory
 * @param file Path to the file
 * @return True if the file is directly located in the directory. False otherwise
 */
bool is_in_directory(const string &directory, const string &file) {
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

vector<string>* split_path(const string &path) {
  vector<string>* tokens = new vector<string>();
  string trimmed_path = clean_path(path);
  while (trimmed_path.length() > 0) {
    size_t start = 0, pos = 0;
    pos = trimmed_path.find("/", start);
    if (pos == string::npos) {
      tokens->push_back(trimmed_path.substr(start, trimmed_path.length()));
      break;
    }
    size_t length = pos - start;
    if (length > 0) {
      string token = trimmed_path.substr(start, length);
      if (token.length() > 0) {
        tokens->push_back(token);
      }
    }
    trimmed_path = clean_path(trimmed_path.substr(pos + 1, string::npos));
  }
  return tokens;
}
