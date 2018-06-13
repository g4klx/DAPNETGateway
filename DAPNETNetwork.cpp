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

#include "DAPNETNetwork.h"
#include "Thread.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

const unsigned int BUFFER_LENGTH = 200U;

CDAPNETNetwork::CDAPNETNetwork(const std::string& address, unsigned int port, const std::string& callsign, const std::string& authKey, const char* version, bool debug) :
m_socket(address, port),
m_callsign(callsign),
m_authKey(authKey),
m_version(version),
m_debug(debug),
m_message(NULL)
{
	assert(!callsign.empty());
	assert(!authKey.empty());
	assert(version != NULL);
}

CDAPNETNetwork::~CDAPNETNetwork()
{
	delete m_message;
}

bool CDAPNETNetwork::open()
{
	LogMessage("Opening DAPNET connection");

	return m_socket.open();
}

bool CDAPNETNetwork::login()
{
	LogMessage("Logging into DAPNET");

	char login[200U];
	::snprintf(login, 200, "[MMDVM v%s %s %s]\r\n", m_version, m_callsign.c_str(), m_authKey.c_str());

	return write((unsigned char*)login);
}

bool CDAPNETNetwork::read()
{
	unsigned char buffer[BUFFER_LENGTH];

	int length = m_socket.read(buffer, BUFFER_LENGTH, 0U);
	if (length == -1)		// Error
		return false;
	if (length == -2)		// Connection lost
		return false;
	if (length == 0)
		return true;

	if (m_debug)
		CUtils::dump(1U, "DAPNET Data Received", buffer, length);

	// Turn it into a C string
	buffer[length] = 0x00U;

	if (buffer[0U] == '+') {
		// Success
	} else if (buffer[0U] == '-') {
		// Error
		LogWarning("An error has been reported by DAPNET");
	} else if (buffer[0U] == '2') {
		// Time synchronisation
		char* p = ::strchr((char*)buffer, '\n');
		if (p != NULL)
			::strcpy(p, ":0000\r\n");
		else
			::strcat((char*)buffer, ":0000\r\n");

		bool ok = write(buffer);
		if (!ok)
			return false;

		return write((unsigned char*)"+\r\n");
	} else if (buffer[0U] == '3') {
		// ???
		return write((unsigned char*)"+\r\n");
	} else if (buffer[0U] == '4') {
		// Timeslot information
		return write((unsigned char*)"+\r\n");
	} else if (buffer[0U] == '#') {
		// A message
		unsigned int id = ::strtoul((char*)buffer + 1U, NULL, 16);

		char* p1 = ::strtok((char*)buffer + 4U, ":\r\n");
		char* p2 = ::strtok(NULL, ":\r\n");
		char* p3 = ::strtok(NULL, ":\r\n");
		char* p4 = ::strtok(NULL, ":\r\n");
		char* p5 = ::strtok(NULL, "\r\n");

		if (p1 == NULL || p2 == NULL || p3 == NULL || p4 == NULL || p5 == NULL) {
			CUtils::dump(3U, "Received a malformed message from DAPNET", buffer, length);

			id = (id + 1U) % 256UL;

			char reply[20U];
			::snprintf(reply, 20U, "#%02X -\r\n", id);
			return write((unsigned char*)reply);
		} else {
			unsigned int type = ::strtoul(p1, NULL, 10);
			unsigned int addr = ::strtoul(p3, NULL, 16);
			unsigned int func = ::strtoul(p4, NULL, 10);

			m_message = new CPOCSAGMessage(type, addr, func, (unsigned char*)p5, ::strlen(p5));

			id = (id + 1U) % 256UL;

			char reply[20U];
			::snprintf(reply, 20U, "#%02X +\r\n", id);
			return write((unsigned char*)reply);
		}
	} else {
		CUtils::dump(3U, "An unknown message from DAPNET", buffer, length);
		return write((unsigned char*)"-\r\n");
	}

	return true;
}

CPOCSAGMessage* CDAPNETNetwork::readMessage()
{
	if (m_message == NULL)
		return NULL;

	CPOCSAGMessage* message = m_message;

	m_message = NULL;

	return message;
}

void CDAPNETNetwork::close()
{
	m_socket.close();

	LogMessage("Closing DAPNET connection");
}

bool CDAPNETNetwork::write(unsigned char* data)
{
	assert(data != NULL);

	unsigned int length = ::strlen((char*)data);

	if (m_debug)
		CUtils::dump(1U, "DAPNET Data Transmitted", data, length);

	bool ok = m_socket.write(data, length);
	if (!ok)
		LogWarning("Error when writing to DAPNET");

	return ok;
}
