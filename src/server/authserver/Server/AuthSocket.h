/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
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

#ifndef _AUTHSOCKET_H
#define _AUTHSOCKET_H

#include "Common.h"
#include "BigNumber.h"
#include "Common.h"

#include <string>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

class SocketAcceptor;

using boost::asio::ip::tcp;

// Handle login commands
class AuthSocket : public boost::enable_shared_from_this<AuthSocket>
{
public:
    typedef boost::shared_ptr<AuthSocket> SessionPtr;

    static SessionPtr create(boost::asio::io_service& io_service) { return SessionPtr(new AuthSocket(io_service)); }

    tcp::socket& socket() { return _socket; }
    const static int s_BYTE_SIZE = 32;

    AuthSocket(boost::asio::io_service& io_service);
    virtual ~AuthSocket();

    virtual void OnRead();
    virtual void OnAccept();
    virtual void OnClose();

    bool _HandleLogonChallenge();
    bool _HandleLogonProof();
    bool _HandleReconnectChallenge();
    bool _HandleReconnectProof();
    bool _HandleRealmList();

    //data transfer handle for patch
    bool _HandleXferResume();
    bool _HandleXferCancel();
    bool _HandleXferAccept();

    void _SetVSFields(const std::string& rI);

    FILE* pPatch;
    ACE_Thread_Mutex patcherLock;

    // Boost wrapper functions
    void handler(const boost::system::error_code& /*error*/, std::size_t /*bytes_transferred*/) { }
    std::string getRemoteAddress() { return socket().remote_endpoint().address().to_string(); }
    uint16 getRemotePort() { return socket().remote_endpoint().port(); }

    void ReadPacket(char* data, size_t bytesToRead) { boost::asio::async_read(socket(), boost::asio::buffer(data, bytesToRead), boost::bind(&AuthSocket::handler, shared_from_this(), boost::asio::placeholders::error(), boost::asio::placeholders::bytes_transferred())); }
    void ReadPacketSkip(size_t bytesToSkip)
    {
        char waste[bytesToSkip];
        boost::asio::async_read(socket(), boost::asio::buffer(waste, bytesToSkip), boost::bind(&AuthSocket::handler, shared_from_this(), boost::asio::placeholders::error(), boost::asio::placeholders::bytes_transferred()));
    }
    void WritePacket(char* data, size_t bytesToWrite) { boost::asio::async_write(socket(), boost::asio::buffer(data, bytesToWrite), boost::bind(&AuthSocket::handler, shared_from_this(), boost::asio::placeholders::error(), boost::asio::placeholders::bytes_transferred())); }

    void ShutdownSocket() { socket().close(); socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both); }

private:
    tcp::socket _socket;

    BigNumber N, s, g, v;
    BigNumber b, B;
    BigNumber K;
    BigNumber _reconnectProof;

    bool _authed;

    std::string _login;

    // Since GetLocaleByName() is _NOT_ bijective, we have to store the locale as a string. Otherwise we can't differ
    // between enUS and enGB, which is important for the patch system
    std::string _localizationName;
    std::string _os;
    uint16 _build;
    uint8 _expversion;
    AccountTypes _accountSecurityLevel;
};

#endif
