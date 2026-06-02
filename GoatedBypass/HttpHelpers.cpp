#include "HttpHelpers.hpp"
#include <boost/algorithm/string.hpp>
#include <zlib.h>

namespace HttpHelpers 
{
    std::string toLower(std::string s)
    {
        boost::algorithm::to_lower(s);
        return s;
    }

    static inline std::string_view trimCrlf(std::string_view sv)
    {
        while (!sv.empty() && (sv.back() == '\r' || sv.back() == '\n')) 
            sv.remove_suffix(1);
        return sv;
    }

    bool iStartsWith(std::string_view s, std::string_view prefix)
    {
        if (s.size() < prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); ++i)
            if (std::tolower(unsigned(s[i])) != std::tolower(unsigned(prefix[i]))) 
                return false;
        return true;
    }

    std::vector<std::string> splitLines(std::string_view headers) 
    {
        std::vector<std::string> out;
        size_t start = 0;
        while (true)
        {
            size_t pos = headers.find("\r\n", start);
            if (pos == std::string::npos) { out.emplace_back(headers.substr(start)); break; }
            out.emplace_back(headers.substr(start, pos - start));
            start = pos + 2;
            if (start >= headers.size()) break;
        }
        return out;
    }

    std::string joinLines(const std::vector<std::string>& lines) 
    {
        std::string s;
        for (auto& l : lines) { s += l; s += "\r\n"; }
        return s;
    }

    bool isHeaderKeyCI(std::string_view line, std::string_view key)
    {
        if (line.size() < key.size() + 1) return false;
        for (size_t i = 0; i < key.size(); ++i) {
            if (std::tolower(unsigned(line[i])) != std::tolower(unsigned(key[i]))) return false;
        }
        return (line.size() > key.size() && line[key.size()] == ':');
    }

    bool headerContainsCI(const std::string& headers, std::string_view tokenCI) 
    {
        return toLower(headers).find(toLower(std::string(tokenCI))) != std::string::npos;
    }

    std::optional<size_t> getContentLengthCI(const std::vector<std::string>& lines) 
    {
        for (auto& l : lines) 
        {
            if (isHeaderKeyCI(l, "Content-Length")) 
            {
                auto pos = l.find(':');
                if (pos != std::string::npos)
                {
                    std::string v = l.substr(pos + 1);
                    boost::algorithm::trim(v);

                    try 
                    {
                        return static_cast<size_t>(std::stoull(v));
                    }
                    catch (...)
                    {
                        return std::nullopt;
                    }
                }
            }
        }
        return std::nullopt;
    }

    void stripHeader(std::vector<std::string>& lines, std::string_view headerKeyCI)
    {
        lines.erase(std::remove_if(lines.begin(), lines.end(), [&](const std::string& l)
            {
            return isHeaderKeyCI(l, std::string(headerKeyCI));
            }), lines.end());
    }

    void setOrReplaceHeader(std::vector<std::string>& lines, std::string key, std::string value) 
    {
        bool replaced = false;
        for (auto& l : lines) 
        {
            if (isHeaderKeyCI(l, key)) 
            {
                l = key + ": " + value;
                replaced = true;
                break; 
            }
        }
        if (!replaced) 
            lines.insert(lines.end() - 1, key + ": " + value);
    }

    bool readUntilHeaderEnd(Tcp::socket& sock, std::string& out)
    {
        out.clear();
        boost::system::error_code ec;
        char buf[4096];
        while (true)
        {
            size_t n = sock.read_some(boost::asio::buffer(buf, sizeof(buf)), ec);
            if (ec) 
                return false;

            out.append(buf, n);

            if (out.find("\r\n\r\n") != std::string::npos) 
                return true;
        }
    }

    bool readExact(Tcp::socket& sock, std::string& out, size_t bytes)
    {
        boost::system::error_code ec;
        size_t remain = bytes;
        std::string buf;
        buf.resize(bytes);
        size_t off = 0;
        while (remain > 0)
        {
            size_t n = sock.read_some(boost::asio::buffer(&buf[off], remain), ec);
            if (ec || n == 0) return false;
            off += n;
            remain -= n;
        }
        out.append(buf);
        return true;
    }

    bool readLine(Ssl::stream<Tcp::socket>& s, std::string& line) 
    {
        line.clear();
        char c;
        boost::system::error_code ec;
        while (true) 
        {
            size_t n = boost::asio::read(s, boost::asio::buffer(&c, 1), ec);
            if (ec || n == 0) return false;
            if (c == '\n') break;
            if (c != '\r') line.push_back(c);
        }
        return true;
    }

    bool readHeadersSSL(Ssl::stream<Tcp::socket>& s, std::string& headers)
    {
        headers.clear();
        char c;
        boost::system::error_code ec;
        std::string last4;
        last4.reserve(4);
        while (true)
        {
            size_t n = boost::asio::read(s, boost::asio::buffer(&c, 1), ec);
            if (ec || n == 0) return false;
            headers.push_back(c);
            last4.push_back(c);
            if (last4.size() > 4) last4.erase(last4.begin());
            if (last4.size() == 4 && last4[0] == '\r' && last4[1] == '\n' && last4[2] == '\r' && last4[3] == '\n')
                break;
        }
        return true;
    }

    bool readExactSSL(Ssl::stream<Tcp::socket>& s, std::string& out, size_t bytes) 
    {
        boost::system::error_code ec;
        size_t remain = bytes;
        std::string buf;
        buf.resize(bytes);
        size_t off = 0;
        while (remain > 0) 
        {
            size_t n = boost::asio::read(s, boost::asio::buffer(&buf[off], remain), ec);
            if (ec || n == 0) 
                return false;
            off += n;
            remain -= n;
        }
        out.append(buf);
        return true;
    }

    bool readChunkedSSL(Ssl::stream<Tcp::socket>& s, std::string& body)
    {
        while (true) 
        {
            std::string sizeLine;
            if (!readLine(s, sizeLine)) return false;
            auto sv = trimCrlf(sizeLine);
            auto semipos = sv.find(';');
            auto size_sv = (semipos == std::string::npos) ? sv : sv.substr(0, semipos);
            size_t chunkSize = std::stoul(std::string(size_sv), nullptr, 16);

            if (chunkSize == 0) 
            {
                std::string trailer;
                if (!readLine(s, trailer)) return false;
                while (!trailer.empty())
                {
                    if (!readLine(s, trailer)) 
                        return false;
                }

                break;
            }

            std::string chunk;
            if (!readExactSSL(s, chunk, chunkSize)) return false;
            body.append(chunk);
            std::string crlf;
            if (!readExactSSL(s, crlf, 2)) return false;
        }
        return true;
    }

    std::string gzipDecompress(const std::string& data) 
    {
        if (data.empty()) return {};
        z_stream zs{};
        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        zs.avail_in = static_cast<uInt>(data.size());
        if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) { return {}; }

        std::string out;
        out.resize(data.size() * 3 + 1024);
        int ret = Z_OK;

        do 
        {
            if (zs.total_out >= out.size()) out.resize(out.size() * 2);
            zs.next_out = reinterpret_cast<Bytef*>(&out[zs.total_out]);
            zs.avail_out = static_cast<uInt>(out.size() - zs.total_out);
            ret = inflate(&zs, Z_NO_FLUSH);
            if (ret == Z_STREAM_END) break;
            if (ret != Z_OK) { inflateEnd(&zs); return {}; }
        } while (ret == Z_OK);

        out.resize(zs.total_out);
        inflateEnd(&zs);
        return out;
    }

    bool hasHeaderCI(const std::vector<std::string>& lines, std::string_view key)
    {
        for (const auto& l : lines) 
            if (isHeaderKeyCI(l, key))
                return true;

        return false;
    }
} 
