#include <cmath>
#include <cstring>
#include <ctime>
#include <ios>
#include <iostream>
#include <sys/time.h>

#include "logging.h"

static void get_time_in_iso8601(char *buffer) {
    int millisec;
    struct tm *tm_info;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    millisec = lrint(tv.tv_usec / 1000.0); // Round to nearest millisec
    if (millisec >= 1000) { // Allow for rounding up to nearest second
        millisec -= 1000;
        tv.tv_sec++;
    }

    tm_info = localtime(&tv.tv_sec);

    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    sprintf(buffer, "%s.%03d", buffer, millisec);
}

Logger::Logger(const string pathname) {
    this->stream.open(pathname, ios::app);
}

Logger::~Logger() {
    this->stream.close();
}

void Logger::info(const string line) {
    char *buffer = new char[31];
    get_time_in_iso8601(buffer);
    this->stream << "[" << buffer << "] INFO:  " << line << endl;
    cout << "[" << buffer << "] INFO:  " << line << endl;
    delete[] buffer;
}

void Logger::error(const string line) {
    char *buffer = new char[31];
    get_time_in_iso8601(buffer);
    this->stream << "[" << buffer << "] ERROR: " << line << endl;
    cerr << "[" << buffer << "] INFO:  " << line << endl;
    delete[] buffer;
}