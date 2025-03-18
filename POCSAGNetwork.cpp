/*
 *   Copyright (C) 2018,2025 by Jonathan Naylor G4KLX
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

#include "POCSAGNetwork.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>


CPOCSAGNetwork::CPOCSAGNetwork(const std::string& localAddress, unsigned short localPort, const std::string& remoteAddress, unsigned short remotePort, bool debug) :
m_socket(localAddress, localPort),
m_addr(),
m_addrLen(0U),
m_debug(debug)
{
	assert(!remoteAddress.empty());
	assert(remotePort > 0U);

	if (CUDPSocket::lookup(remoteAddress, remotePort, m_addr, m_addrLen) != 0)
		m_addrLen = 0U;
}

CPOCSAGNetwork::~CPOCSAGNetwork()
{
}

bool CPOCSAGNetwork::open()
{
	if (m_addrLen == 0U) {
		LogError("Unable to resolve the address of the host");
		return false;
	}

	LogMessage("Opening POCSAG network connection");

	return m_socket.open(m_addr);
}

bool CPOCSAGNetwork::write(CPOCSAGMessage* message)
{
	assert(message != nullptr);

	unsigned char data[200U];
	data[0U] = 'P';
	data[1U] = 'O';
	data[2U] = 'C';
	data[3U] = 'S';
	data[4U] = 'A';
	data[5U] = 'G';

	data[6U] = message->m_ric >> 16;
	data[7U] = message->m_ric >> 8;
	data[8U] = message->m_ric >> 0;

	data[9U] = message->m_functional;

	::memcpy(data + 10U, message->m_message, message->m_length);

	if (m_debug)
		CUtils::dump(1U, "POCSAG Network Data Sent", data, message->m_length + 10U);

	return m_socket.write(data, message->m_length + 10U, m_addr, m_addrLen);
}

unsigned int CPOCSAGNetwork::read(unsigned char* data)
{
	assert(data != nullptr);

	sockaddr_storage address;
	unsigned int addrLen;
	int length = m_socket.read(data, 1U, address, addrLen);
	if (length <= 0)
		return 0U;

	if (!CUDPSocket::match(address, m_addr)) {
		LogWarning("Received a packet from an unknown address");
		return 0U;
	}

	if (m_debug)
		CUtils::dump(1U, "POCSAG Network Data Received", data, length);

	return length;
}

void CPOCSAGNetwork::close()
{
	m_socket.close();

	LogMessage("Closing POCSAG network connection");
}
