#pragma once
#include <string>
#include <fstream>
#include <mutex>

class Logger
{
public:
    static Logger& get();

    void logRequest(const std::string& host, const std::string& headers, const std::string& body);
    void logResponse(const std::string& host, const std::string& endpoint, const std::string& headers, const std::string& body);

private:
    Logger();
    void write(const std::string& text);

    std::ofstream m_file;
    std::mutex    m_mutex;
};
