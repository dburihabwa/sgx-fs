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
