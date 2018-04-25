#ifndef FUSEGX_LOGGING_H
#define FUSEGX_LOGGING_H

#include <fstream>
#include <string>

using namespace std;


class Logger {
public:
    Logger(const string pathname);
    ~Logger();

    void error(const string line);
    void info(const string line);
private:
    ofstream stream;
};

#endif //FUSEGX_LOGGING_H
