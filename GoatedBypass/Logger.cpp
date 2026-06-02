#include "Logger.hpp"
#include <sstream>
#include <ctime>
#include <iomanip>

static std::string timestamp()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S");
    return ss.str();
}

static std::string truncate(const std::string& s, size_t maxLen = 4096)
{
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen) + "\n... [truncated, total=" + std::to_string(s.size()) + " bytes]";
}

Logger& Logger::get()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
{
    m_file.open("riot_traffic.log", std::ios::out | std::ios::trunc);
    write("=== GoatedBypass Traffic Log ===\n");
}

void Logger::write(const std::string& text)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open())
    {
        m_file << text;
        m_file.flush();
    }
}

void Logger::logRequest(const std::string& host,
                        const std::string& headers,
                        const std::string& body)
{
    std::ostringstream ss;
    ss << "\n[" << timestamp() << "] >>> REQUEST to " << host << "\n";
    ss << "---- HEADERS ----\n" << truncate(headers) << "\n";
    if (!body.empty())
        ss << "---- BODY ----\n" << truncate(body) << "\n";
    ss << "-----------------\n";
    write(ss.str());
}

void Logger::logResponse(const std::string& host,
                         const std::string& endpoint,
                         const std::string& headers,
                         const std::string& body)
{
    std::ostringstream ss;
    ss << "\n[" << timestamp() << "] <<< RESPONSE from " << host << " [" << endpoint << "]\n";
    ss << "---- HEADERS ----\n" << truncate(headers) << "\n";
    if (!body.empty())
        ss << "---- BODY ----\n" << truncate(body) << "\n";
    ss << "-----------------\n";
    write(ss.str());
}
