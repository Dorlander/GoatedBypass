#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string>


class ConfigProxy
{
public:
    ConfigProxy();

public:
    void run();
    void stop();

	uint16_t getPort() const { return m_acceptor.local_endpoint().port(); }

private:
    using Tcp = boost::asio::ip::tcp;

    void handleClient(Tcp::socket client);
    bool forwardServerToClient(boost::asio::ssl::stream<Tcp::socket>& server, Tcp::socket& client, const std::string& endpoint);
    void replaceHostAndOrigin(std::string& headerText, const std::string& targetHost);

    boost::asio::io_context m_io;
    Tcp::acceptor m_acceptor;
    boost::asio::ssl::context m_sslCtx;
    std::atomic<bool> m_stopFlag = false;
};
