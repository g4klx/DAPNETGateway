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

#include "DAPNETNetwork.h"
#include "Thread.h"
#include "Utils.h"
#include "Log.h"

#include <algorithm>

#include <cstdio>
#include <cassert>
#include <cstring>

const unsigned int BACKOFF[] = { 2000U, 4000U, 8000U, 10000U, 20000U, 60000U, 120000U, 240000U, 480000U, 600000U };

const unsigned int BUFFER_LENGTH = 200U;

CDAPNETNetwork::CDAPNETNetwork(const std::string& address, unsigned short port, const std::string& callsign, const std::string& authKey, const char* version, bool loggedIn, int failCount, bool debug) :
m_socket(address, port),
m_callsign(callsign),
m_authKey(authKey),
m_version(version),
m_loggedIn(false),
m_failCount(failCount),
m_debug(debug),
m_message(nullptr),
m_schedule(nullptr)
{
	assert(!callsign.empty());
	assert(!authKey.empty());
	assert(version != nullptr);
}

CDAPNETNetwork::~CDAPNETNetwork()
{
	delete   m_message;
	delete[] m_schedule;
}

bool CDAPNETNetwork::open()
{
	LogMessage("Opening DAPNET connection");

	return m_socket.open();
}

bool CDAPNETNetwork::login()
{
	LogMessage("Logging into DAPNET");

	std::transform(m_callsign.begin(), m_callsign.end(), m_callsign.begin(), ::tolower);

	char login[200U];
	::snprintf(login, 200, "[DAPNETGateway v%s %s %s]\r\n", m_version, m_callsign.c_str(), m_authKey.c_str());

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
		// First time sync paket indicated successful login
		if (!m_loggedIn) {
			m_loggedIn = true;
			LogMessage("Logged into the DAPNET network");
		}
		// Time synchronisation
		char* p = ::strchr((char*)buffer, '\n');
		if (p != nullptr)
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
		return parseSchedule(buffer);
	} else if (buffer[0U] == '7') {
		// Login failed
		return parseFailedLogin(buffer);
	} else if (buffer[0U] == '#') {
		// A message
		return parseMessage(buffer, length);
	} else {
		CUtils::dump(3U, "An unknown message from DAPNET", buffer, length);
		return write((unsigned char*)"-\r\n");
	}

	return true;
}

bool* CDAPNETNetwork::readSchedule()
{
	bool* schedule = m_schedule;

	m_schedule = nullptr;

	return schedule;
}

CPOCSAGMessage* CDAPNETNetwork::readMessage()
{
	if (m_message == nullptr)
		return nullptr;

	CPOCSAGMessage* message = m_message;

	m_message = nullptr;

	return message;
}

void CDAPNETNetwork::close()
{
	m_socket.close();

	LogMessage("Closing DAPNET connection");
}

bool CDAPNETNetwork::write(unsigned char* data)
{
	assert(data != nullptr);

	unsigned int length = (unsigned int)::strlen((char*)data);

	if (m_debug)
		CUtils::dump(1U, "DAPNET Data Transmitted", data, length);

	bool ok = m_socket.write(data, length);
	if (!ok)
		LogWarning("Error when writing to DAPNET");

	return ok;
}

bool CDAPNETNetwork::parseMessage(unsigned char* buffer, unsigned int length)
{
	assert(buffer != nullptr);

	unsigned int id = ::strtoul((char*)buffer + 1U, nullptr, 16);

	char* p1 = ::strtok((char*)buffer + 4U, ":\r\n");
	char* p2 = ::strtok(nullptr, ":\r\n");
	char* p3 = ::strtok(nullptr, ":\r\n");
	char* p4 = ::strtok(nullptr, ":\r\n");
	char* p5 = ::strtok(nullptr, "\r\n");

	if (p1 == nullptr || p2 == nullptr || p3 == nullptr || p4 == nullptr || p5 == nullptr) {
		CUtils::dump(3U, "Received a malformed message from DAPNET", buffer, length);

		id = (id + 1U) % 256UL;

		char reply[20U];
		::snprintf(reply, 20U, "#%02X -\r\n", id);
		return write((unsigned char*)reply);
	} else {
		unsigned int type = ::strtoul(p1, nullptr, 10);
		unsigned int addr = ::strtoul(p3, nullptr, 16);
		unsigned int func = ::strtoul(p4, nullptr, 10);

		m_message = new CPOCSAGMessage(type, addr, func, (unsigned char*)p5, (unsigned int)::strlen(p5));

		id = (id + 1U) % 256UL;

		char reply[20U];
		::snprintf(reply, 20U, "#%02X +\r\n", id);
		return write((unsigned char*)reply);
	}
}

bool CDAPNETNetwork::parseSchedule(unsigned char* data)
{
	assert(data != nullptr);

	char* p = ::strtok((char*)data + 2U, "\r\n");
	assert(p != nullptr);

	LogMessage("Schedule information received: %s", p);

	delete[] m_schedule;
	m_schedule = new bool[16U];

	for (unsigned int i = 0U; i < 16U; i++)
		m_schedule[i] = false;

	if (::strchr(p, '0') != nullptr)
		m_schedule[0U] = true;
	if (::strchr(p, '1') != nullptr)
		m_schedule[1U] = true;
	if (::strchr(p, '2') != nullptr)
		m_schedule[2U] = true;
	if (::strchr(p, '3') != nullptr)
		m_schedule[3U] = true;
	if (::strchr(p, '4') != nullptr)
		m_schedule[4U] = true;
	if (::strchr(p, '5') != nullptr)
		m_schedule[5U] = true;
	if (::strchr(p, '6') != nullptr)
		m_schedule[6U] = true;
	if (::strchr(p, '7') != nullptr)
		m_schedule[7U] = true;
	if (::strchr(p, '8') != nullptr)
		m_schedule[8U] = true;
	if (::strchr(p, '9') != nullptr)
		m_schedule[9U] = true;
	if (::strchr(p, 'A') != nullptr)
		m_schedule[10U] = true;
	if (::strchr(p, 'B') != nullptr)
		m_schedule[11U] = true;
	if (::strchr(p, 'C') != nullptr)
		m_schedule[12U] = true;
	if (::strchr(p, 'D') != nullptr)
		m_schedule[13U] = true;
	if (::strchr(p, 'E') != nullptr)
		m_schedule[14U] = true;
	if (::strchr(p, 'F') != nullptr)
		m_schedule[15U] = true;

	return write((unsigned char*)"+\r\n");
}

bool CDAPNETNetwork::parseFailedLogin(unsigned char* data)
{
	assert(data != nullptr);

	char* p = ::strtok((char*)data + 2U, "\r\n");
	assert(p != nullptr);

	LogMessage("Login failed: %s", p);

	CThread::sleep(BACKOFF[m_failCount]);
	if (m_failCount < 9)
		m_failCount++;

	return write((unsigned char*)"+\r\n");
}
