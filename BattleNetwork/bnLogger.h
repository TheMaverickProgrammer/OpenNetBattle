#pragma once
#include <iostream>
#include <string>
#include <cstdarg>
#include <queue>
#include <mutex>
#include <fstream>
#include "bnCurrentTime.h"
#include <filesystem>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

using std::string;
using std::to_string;
using std::cerr;
using std::endl;

namespace LogLevel {
  const uint8_t silent = 0x00;
  const uint8_t info = 0x01;
  const uint8_t debug = 0x02;
  const uint8_t warning = 0x04;
  const uint8_t critical = 0x08;
  const uint8_t net = 0x10;
  const uint8_t all = info | warning | critical | debug | net;
};

/*! \brief Thread safe logging utility logs directly to a file */
class Logger {
private:
  static std::mutex m;
  static std::queue<std::string> logs; /*!< get the log stream in order */
  static std::ofstream file; /*!< The file to write to */
  static uint8_t logLevel;

public:

  /**
  * @breif sets the log level filter so that any message that does not match will not be reported
  */
  static void SetLogLevel(uint8_t level) {
    logLevel = level;
  }

  /**
   * @brief Gets the next log and stores it in the input string
   * @param next input string to store result into
   * @return true if there's more text. False if there's no text to INPUTx.
   */
  static const bool GetNextLog(std::string &next) {
    std::scoped_lock<std::mutex> lock(m);

    if (logs.size() == 0)
        return false;

    next = logs.front();
    logs.pop();

    return (logs.size()+1 > 0);
  }

  /**
    TODO: Proper comment
  
    Closes the current file and creates a new one, changing the 
    file pointer. Appends a timestamp to the new file's name.
  */
  static const void StartNewLog() {
    std::scoped_lock<std::mutex> lock(m);

    std::filesystem::path dir = "logs";
    std::filesystem::create_directory(dir);

    std::string logName = "logs/log.txt";
    int num = 0;

    while (std::filesystem::exists(logName)) {
      num = num + 1;
      logName = "logs/log_" + std::to_string(num) + ".txt";
    }

    file.close();
    
    file.open(logName, std::ios::app);
    file << "==============================" << endl;
    file << "StartTime " << CurrentTime::AsString() << endl;
  }

  /**
   * @brief If first time opening, timestamps file and pushes message to file
   * @param _message
   */
  static void Log(uint8_t level, string _message) {
    std::scoped_lock<std::mutex> lock(m);

    if (_message.empty())
      return;

    _message = ErrorLevel(level) + _message;

    if (!file.is_open()) {
      file.open("log.txt", std::ios::app);
      file << "==============================" << endl;
      file << "StartTime " << CurrentTime::AsString() << endl;
    }

#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "open mmbn engine", "%s", _message.c_str());
#else
    if ((level & Logger::logLevel) == level) {
      cerr << _message << endl;
    }
#endif

    logs.push(_message);
    file << _message << endl;
  }

  /**
   * @brief Uses varadic args to print any string format
   * @param fmt string format
   * @param ... input to match the format
   */
  static void Logf(uint8_t level, const char* fmt, ...) {
    std::scoped_lock<std::mutex> lock(m);

    int size = 512;
    char* buffer = 0;
    buffer = new char[static_cast<size_t>(size)];
    va_list vl, vl2;
    va_start(vl, fmt);
    va_copy(vl2, vl);
    int nsize = vsnprintf(buffer, size, fmt, vl);
    if (size <= nsize) {
      delete[] buffer;
      buffer = new char[static_cast<size_t>(nsize) + 1];
      nsize = vsnprintf(buffer, size, fmt, vl2);
    }
    std::string ret(buffer);
    va_end(vl);
    va_end(vl2);
    delete[] buffer;

    ret = ErrorLevel(level) + ret;

    // only print what level of error message we want to console
    if ((level & Logger::logLevel) == level) {
      cerr << ret << endl;
    }

    logs.push(ret);

#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO,"open mmbn engine","%s",ret.c_str());
#else
    if (!file.is_open()) {
      file.open("log.txt", std::ios::app);
      file << "==========================" << endl;
      file << "StartTime " << CurrentTime::AsString() << endl;
    }

    file << ret << endl;
#endif
  }

private:
  Logger() { ; }

  /**
   * @brief Dumps queue and closes file
   */
  ~Logger() { file.close(); while (!logs.empty()) { logs.pop(); } }

  static std::string ErrorLevel(uint8_t level) {
    if (level == LogLevel::critical) {
      return "[CRITICAL] ";
    }

    if (level == LogLevel::warning) {
      return "[WARNING] ";
    }

    if (level == LogLevel::debug) {
      return "[DEBUG] ";
    }

    if (level == LogLevel::net) {
      return "[NET] ";
    }

    // anything else
    return "[INFO] ";
  }
};
