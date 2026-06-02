#include "ConfigProxy.hpp"
#include "HttpHelpers.hpp"
#include "JsonPatcher.hpp"
#include "Logger.hpp"

#include <openssl/ssl.h>
#include <iostream>

using namespace HttpHelpers;

ConfigProxy::ConfigProxy()
    : m_io()
    , m_acceptor(m_io)
    , m_sslCtx(boost::asio::ssl::context::tlsv12_client)
{
    m_sslCtx.set_default_verify_paths();
    m_sslCtx.set_verify_mode(boost::asio::ssl::verify_none);
    m_sslCtx.set_options(boost::asio::ssl::context::default_workarounds);
}

void ConfigProxy::run() 
{
    Tcp::endpoint ep(boost::asio::ip::make_address("127.0.0.1"),
        static_cast<unsigned short>(0));

    m_acceptor.open(ep.protocol());
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    m_acceptor.bind(ep);
    m_acceptor.listen();

    while (!m_stopFlag.load(std::memory_order_relaxed))
    {
        boost::system::error_code ec;
        Tcp::socket client(m_io);
        m_acceptor.accept(client, ec);

        if (ec)
        {
            if (m_stopFlag) break;
            continue;
        }
        std::thread(&ConfigProxy::handleClient, this, std::move(client)).detach();
    }
}

void ConfigProxy::stop()
{
    m_stopFlag.store(true, std::memory_order_relaxed);
    boost::system::error_code ec;
    m_acceptor.close(ec);
}

void ConfigProxy::replaceHostAndOrigin(std::string& headerText, const std::string& targetHost)
{
    auto lines = splitLines(headerText);

    for (auto& l : lines)
    {
        if (isHeaderKeyCI(l, "Host")) 
            l = "Host: " + targetHost;

        else if (isHeaderKeyCI(l, "Origin")) 
            l = "Origin: https://" + targetHost;
    }

    headerText = joinLines(lines);
}

bool ConfigProxy::forwardServerToClient(Ssl::stream<Tcp::socket>& server, Tcp::socket& client, const std::string& endpoint)
{
    std::string srvHeaders;
    if (!readHeadersSSL(server, srvHeaders)) return false;

    bool isNoContent = iStartsWith(srvHeaders, "HTTP/1.1 204") ||
        iStartsWith(srvHeaders, "HTTP/2 204") ||
        headerContainsCI(srvHeaders, "Content-Length: 0");

    if (isNoContent) 
    {
        boost::asio::write(client, boost::asio::buffer(srvHeaders));
        return true;
    }

    auto headerEnd = srvHeaders.find("\r\n\r\n");
    std::string headerSection = (headerEnd == std::string::npos) ? srvHeaders : srvHeaders.substr(0, headerEnd + 4);
    std::string extraBody = (headerEnd == std::string::npos) ? std::string() : srvHeaders.substr(headerEnd + 4);

    auto lines = splitLines(headerSection);
    if (!lines.empty() && lines.back().empty()) lines.pop_back();

    bool isChunked = hasHeaderCI(lines, "Transfer-Encoding");
    bool isGzip = false;
    for (auto& l : lines) {
        if (isHeaderKeyCI(l, "Transfer-Encoding") && toLower(l).find("chunked") != std::string::npos) 
            isChunked = true;
        if (isHeaderKeyCI(l, "Content-Encoding") && toLower(l).find("gzip") != std::string::npos) 
            isGzip = true;
    }

    if (isChunked) stripHeader(lines, "Transfer-Encoding");
    stripHeader(lines, "Content-Length");

    std::string body;
    body.swap(extraBody);

    if (isChunked) 
    {
        if (!readChunkedSSL(server, body)) return false;
    }
    else 
    {
        auto origLines = splitLines(headerSection);
        auto origCL = getContentLengthCI(origLines);
        if (origCL && body.size() < *origCL) {
            std::string more;
            if (!readExactSSL(server, more, *origCL - body.size())) return false;
            body += more;
        }
    }

    if (isGzip)
    {
        stripHeader(lines, "Content-Encoding");
        auto decompressed = gzipDecompress(body);

        if (decompressed.empty() && !body.empty())
        {
            lines.push_back("");
            auto hdr = joinLines(lines);
            boost::asio::write(client, boost::asio::buffer(hdr));
            boost::asio::write(client, boost::asio::buffer(body));
            return true;
        }

        Logger::get().logResponse("clientconfig.rpg.riotgames.com", endpoint, headerSection, decompressed);

        auto modified = JsonPatcher::patchNovgkOnly(decompressed);
        setOrReplaceHeader(lines, "Content-Length", std::to_string(modified.size()));
        lines.push_back("");
        auto hdr = joinLines(lines);
        boost::asio::write(client, boost::asio::buffer(hdr));
        if (!modified.empty())
            boost::asio::write(client, boost::asio::buffer(modified));
        return true;
    }
    else
    {
        Logger::get().logResponse("clientconfig.rpg.riotgames.com", endpoint, headerSection, body);

        auto modified = JsonPatcher::patchNovgkOnly(body);
        setOrReplaceHeader(lines, "Content-Length", std::to_string(modified.size()));

        lines.push_back("");
        auto hdr = joinLines(lines);
        boost::asio::write(client, boost::asio::buffer(hdr));
        if (!modified.empty()) 
            boost::asio::write(client, boost::asio::buffer(modified));
        return true;
    }
}

void ConfigProxy::handleClient(Tcp::socket client)
{
    try 
    {
        const std::string targetHost = "clientconfig.rpg.riotgames.com";
        std::unique_ptr<Ssl::stream<Tcp::socket>> sslStream;

        while (!m_stopFlag.load(std::memory_order_relaxed))
        {
            std::string req;
            if (!readUntilHeaderEnd(client, req)) break;

            auto headerEnd = req.find("\r\n\r\n");
            if (headerEnd == std::string::npos) break;
            std::string headersText = req.substr(0, headerEnd + 4);
            std::string alreadyBody = req.substr(headerEnd + 4);

            std::string endpoint;
            {
                auto pos = headersText.find("\r\n");
                std::string first = (pos == std::string::npos) ? headersText : headersText.substr(0, pos);
                auto p1 = first.find(' ');
                auto p2 = (p1 == std::string::npos) ? std::string::npos : first.find(' ', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                    endpoint = first.substr(p1 + 1, p2 - (p1 + 1));
                }
            }

            auto lines = splitLines(headersText);
            if (!lines.empty() && lines.back().empty()) lines.pop_back();

            size_t contentLength = 0;
            if (auto cl = getContentLengthCI(lines)) contentLength = *cl;

            std::string body = std::move(alreadyBody);
            if (contentLength > body.size())
            {
                std::string more;
                if (!readExact(client, more, contentLength - body.size())) break;
                body += more;
            }

            if (!sslStream)
            {
                boost::asio::ip::tcp::resolver resolver(m_io);
                auto results = resolver.resolve(targetHost, "443");
                sslStream = std::make_unique<Ssl::stream<Tcp::socket>>(m_io, m_sslCtx);
                boost::asio::connect(sslStream->next_layer(), results);

                SSL_set_tlsext_host_name(sslStream->native_handle(), targetHost.c_str());
                sslStream->set_verify_mode(Ssl::verify_none);
                sslStream->handshake(Ssl::stream_base::client);
            }

            Logger::get().logRequest(targetHost, headersText, body);

            std::string modHeaders = headersText;
            replaceHostAndOrigin(modHeaders, targetHost);

            boost::asio::write(*sslStream, boost::asio::buffer(modHeaders));
            if (!body.empty()) boost::asio::write(*sslStream, boost::asio::buffer(body));

            if (!forwardServerToClient(*sslStream, client, endpoint))
                break;
        }
    }
    catch (...)
    {

    }

    boost::system::error_code ec;
    client.shutdown(Tcp::socket::shutdown_both, ec);
    client.close(ec);
}

