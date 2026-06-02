#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace HttpHelpers
{
	using Tcp = boost::asio::ip::tcp;
	namespace Ssl = boost::asio::ssl;

	std::string toLower(std::string s);

	bool iStartsWith(std::string_view s, std::string_view prefix);
	std::vector<std::string> splitLines(std::string_view headers);
	std::string joinLines(const std::vector<std::string>& lines);
	bool isHeaderKeyCI(std::string_view line, std::string_view key);
	bool headerContainsCI(const std::string& headers, std::string_view tokenCI);
	std::optional<size_t> getContentLengthCI(const std::vector<std::string>& lines);
	void stripHeader(std::vector<std::string>& lines, std::string_view headerKeyCI);
	void setOrReplaceHeader(std::vector<std::string>& lines, std::string key, std::string value);

	bool readUntilHeaderEnd(Tcp::socket& sock, std::string& out);
	bool readExact(Tcp::socket& sock, std::string& out, size_t bytes);

	bool readLine(Ssl::stream<Tcp::socket>& s, std::string& line);
	bool readHeadersSSL(Ssl::stream<Tcp::socket>& s, std::string& headers);
	bool readExactSSL(Ssl::stream<Tcp::socket>& s, std::string& out, size_t bytes);
	bool readChunkedSSL(Ssl::stream<Tcp::socket>& s, std::string& body);

	std::string gzipDecompress(const std::string& data);
	bool hasHeaderCI(const std::vector<std::string>& lines, std::string_view key);
} 
