/*
 *   Copyright (C) 2018,2023 by Jonathan Naylor G4KLX
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

#if !defined(CONF_H)
#define	CONF_H

#include <string>
#include <vector>

class CConf
{
public:
	CConf(const std::string& file);
	~CConf();

	bool read();

	// The General section
	std::string  getCallsign() const;
	std::vector<unsigned int> getWhiteList() const;
	std::vector<unsigned int> getBlackList() const;
	std::string  getblacklistRegexfile() const;
	std::string  getwhitelistRegexfile() const;
	std::string  getRptAddress() const;
	unsigned short getRptPort() const;
	std::string  getMyAddress() const;
	unsigned short getMyPort() const;
	bool         getDaemon() const;

	// The Log section
	unsigned int getLogDisplayLevel() const;
	unsigned int getLogMQTTLevel() const;

	// The MQTT section
	std::string    getMQTTAddress() const;
	unsigned short getMQTTPort() const;
	unsigned int   getMQTTKeepalive() const;
	std::string    getMQTTName() const;

	// The DAPNET section
	std::string  getDAPNETAddress() const;
	unsigned short getDAPNETPort() const;
	std::string  getDAPNETAuthKey() const;
	bool         getDAPNETDebug() const;

private:
	std::string  m_file;

	std::string  m_callsign;
	std::vector<unsigned int> m_whiteList;
	std::vector<unsigned int> m_blackList;

	std::string  m_blacklistRegexfile;
	std::string  m_whitelistRegexfile;
	std::string  m_rptAddress;
	unsigned short m_rptPort;
	std::string  m_myAddress;
	unsigned short m_myPort;
	bool         m_daemon;

	unsigned int m_logDisplayLevel;
	unsigned int m_logMQTTLevel;

	std::string    m_mqttAddress;
	unsigned short m_mqttPort;
	unsigned int   m_mqttKeepalive;
	std::string    m_mqttName;

	std::string  m_dapnetAddress;
	unsigned short m_dapnetPort;
	std::string  m_dapnetAuthKey;
	bool         m_dapnetDebug;
};

#endif
