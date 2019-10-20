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

#ifndef	POCSAGNetwork_H
#define	POCSAGNetwork_H

#include "POCSAGMessage.h"
#include "UDPSocket.h"

#include <cstdint>
#include <string>

class CPOCSAGNetwork {
public:
	CPOCSAGNetwork(const std::string& localAddress, unsigned int localPort, const std::string& remoteAddress, unsigned int remotePort, bool debug);
	~CPOCSAGNetwork();

	bool open();

	bool write(CPOCSAGMessage* message);

	unsigned int read(unsigned char* data);

	void close();

private:
	CUDPSocket       m_socket;
	sockaddr_storage m_address;
	unsigned int     m_addrlen;
	bool             m_debug;
};

#endif
