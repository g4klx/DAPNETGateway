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

#if !defined(DAPNETGateway_H)
#define	DAPNETGateway_H

#include "DAPNETNetwork.h"
#include "POCSAGNetwork.h"
#include "POCSAGMessage.h"
#include "StopWatch.h"
#include "Conf.h"

#include <string>
#include <deque>
#include <vector>

class CDAPNETGateway
{
public:
	CDAPNETGateway(const std::string& configFile);
	~CDAPNETGateway();

	int run();

private:
	CConf                       m_conf;
	CDAPNETNetwork*             m_dapnetNetwork;
	CPOCSAGNetwork*             m_pocsagNetwork;
	std::deque<CPOCSAGMessage*> m_queue;
	CStopWatch                  m_slotTimer;
	bool*                       m_schedule;
	unsigned int                m_currentSlot;
	unsigned int                m_sentCodewords;
	bool                        m_mmdvmFree;


	void sendMessages();
	bool recover();
	bool isTimeMessage(const CPOCSAGMessage* message) const;
	unsigned int calculateCodewords(const CPOCSAGMessage* message) const;
	void loadSchedule();
};

#endif

