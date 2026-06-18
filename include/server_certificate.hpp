#pragma once
#include <boost/asio/ssl.hpp>
#include <string>
#include <fstream>
#include <streambuf>
#include <iostream>

inline void load_server_certificate(boost::asio::ssl::context& ctx)
{
    ctx.set_password_callback(
        [](std::size_t, boost::asio::ssl::context_base::password_purpose)
        {
            return "test";
        });

    ctx.use_certificate_chain_file("server.crt");
    ctx.use_private_key_file("server.key", boost::asio::ssl::context::pem);
}
