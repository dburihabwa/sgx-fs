#ifndef FUSEGX_LOGGING_H
#define FUSEGX_LOGGING_H

#include <fstream>
#include <string>

using namespace std;


class Logger {
public:
    Logger(const string pathname);

    ~Logger();

    void error(const string &line);

    void info(const string &line);

private:
    ofstream stream;
};


class NoOpLogger {
public:
    inline NoOpLogger() {}
    inline void error(const string &) {}
    inline void info(const string &) {}
};

string convert_pointer_to_string(const void *pointer);

#endif //FUSEGX_LOGGING_H
