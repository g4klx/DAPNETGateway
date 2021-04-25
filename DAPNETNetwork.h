/*
 *   Copyright (C) 2018 by Jonathan Naylor G4KLX
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef	DAPNETNetwork_H
#define	DAPNETNetwork_H

#include "POCSAGMessage.h"
#include "TCPSocket.h"
#include "Timer.h"

#include <cstdint>
#include <string>

class CDAPNETNetwork {
public:
	CDAPNETNetwork(const std::string& address, unsigned short port, const std::string& callsign, const std::string& authKey, const char* version, bool loggedIn, int failCount, bool debug);
	~CDAPNETNetwork();

	bool open();

	bool login();

	bool read();

	bool* readSchedule();

	CPOCSAGMessage* readMessage();

	void close();

private:
	CTCPSocket      m_socket;
	std::string     m_callsign;
	std::string     m_authKey;
	const char*     m_version;
	bool            m_loggedIn;
	int             m_failCount;
	bool            m_debug;
	CPOCSAGMessage* m_message;
	bool*           m_schedule;

	bool parseMessage(unsigned char* data, unsigned int length);
	bool parseSchedule(unsigned char* data);
	bool parseFailedLogin(unsigned char* data);
	bool write(unsigned char* data);
};

#endif
