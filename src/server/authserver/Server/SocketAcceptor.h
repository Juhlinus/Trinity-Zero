/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ___socketacceptorH__
#define ___socketacceptorH__

#include "Common.h"
#include "AuthSocket.h"
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class SocketAcceptor
{
public:
    SocketAcceptor(boost::asio::io_service& io_service, uint16 port) : _acceptor(io_service, tcp::endpoint(tcp::v4(), port))
    {
        start_accept();
    }

private:
    void start_accept()
    {
        AuthSocket::SessionPtr newConnection = AuthSocket::create(_acceptor.get_io_service());
        _acceptor.async_accept(newConnection->socket(), boost::bind(&SocketAcceptor::handle_accept, this,
                                                                    newConnection, boost::asio::placeholders::error));
    }

    void handle_accept(AuthSocket::SessionPtr newConnection, const boost::system::error_code& error)
    {
        if (!error)
        {
            newConnection->OnAccept();
            start_accept();
        }
    }

    tcp::acceptor _acceptor;
};

#endif
