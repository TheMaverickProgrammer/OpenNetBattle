#pragma once
#include <iostream>
using std::cout;
using std::endl;
using std::cin;
#include <string>
using std::string;
using std::to_string;
#include <cstdarg>
#include <queue>
#include <mutex>
class Logger {
private:
  static std::mutex m;
  static std::queue<std::string> logs;

public:
  static std::mutex* GetMutex() {
    return &m;
  }

  static const bool GetNextLog(std::string &next) {
    if (logs.size() == 0)
      return false;

    next = logs.front();
    logs.pop();

    return (logs.size() > 0);
  }

  static void Log(string _message) {
    logs.push(_message);
  }

  static void Logf(const char* fmt, ...) {
    int size = 512;
    char* buffer = 0;
    buffer = new char[size];
    va_list vl;
    va_start(vl, fmt);
    int nsize = vsnprintf(buffer, size, fmt, vl);
    if (size <= nsize) {
      delete[] buffer;
      buffer = 0;
      buffer = new char[nsize + 1];
      nsize = vsnprintf(buffer, size, fmt, vl);
    }
    std::string ret(buffer);
    va_end(vl);
    delete[] buffer;
    logs.push(ret);
  }

  template<typename ... Args>
  static void Failf(const string& _format, Args... args) {
    fprintf_s(stderr, ("ERROR: " + _format).c_str(), args...);
    cin.ignore(); // Allows us to see the error before the window closes
    exit(EXIT_FAILURE);
  }

private:
  Logger();
  ~Logger() = default;
};