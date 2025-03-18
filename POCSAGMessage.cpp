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

#include "POCSAGMessage.h"

#include <cstdio>
#include <cassert>
#include <cstring>

CPOCSAGMessage::CPOCSAGMessage(unsigned char type, unsigned int ric, unsigned char functional, unsigned char* message, unsigned int length) :
m_type(type),
m_ric(ric),
m_functional(functional),
m_message(nullptr),
m_length(length),
m_timeQueued()
{
	assert(functional < 4U);
	assert(message != nullptr);
	assert(length > 0U);

	m_message = new unsigned char[length + 1];

	::memcpy(m_message, message, length);
	m_message[length] = '\0';

	m_timeQueued.start();
}

CPOCSAGMessage::~CPOCSAGMessage()
{
	delete[] m_message;
}
