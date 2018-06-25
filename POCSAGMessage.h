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

#ifndef	POCSAGMessage_H
#define	POCSAGMessage_H

#include "StopWatch.h"

#include <cstdint>
#include <string>

class CPOCSAGMessage {
public:
	CPOCSAGMessage(unsigned char type, unsigned int ric, unsigned char functional, unsigned char* message, unsigned int length);
	~CPOCSAGMessage();

	unsigned char  m_type;
	unsigned int   m_ric;
	unsigned char  m_functional;
	unsigned char* m_message;
	unsigned int   m_length;
	CStopWatch     m_timeQueued;
};

#endif
