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

#include "Conf.h"
#include "Log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

const int BUFFER_SIZE = 500;

enum SECTION {
  SECTION_NONE,
  SECTION_GENERAL,
  SECTION_LOG,
  SECTION_DAPNET
};

CConf::CConf(const std::string& file) :
m_file(file),
m_callsign(),
m_whiteList(),
m_rptAddress(),
m_rptPort(0U),
m_myAddress(),
m_myPort(0U),
m_daemon(false),
m_logDisplayLevel(0U),
m_logFileLevel(0U),
m_logFilePath(),
m_logFileRoot(),
m_dapnetAddress(),
m_dapnetPort(0U),
m_dapnetAuthKey(),
m_dapnetDebug(false)
{
}

CConf::~CConf()
{
}

bool CConf::read()
{
  FILE* fp = ::fopen(m_file.c_str(), "rt");
  if (fp == NULL) {
    ::fprintf(stderr, "Couldn't open the .ini file - %s\n", m_file.c_str());
    return false;
  }

  SECTION section = SECTION_NONE;

  char buffer[BUFFER_SIZE];
  while (::fgets(buffer, BUFFER_SIZE, fp) != NULL) {
    if (buffer[0U] == '#')
      continue;

    if (buffer[0U] == '[') {
      if (::strncmp(buffer, "[General]", 9U) == 0)
        section = SECTION_GENERAL;
	  else if (::strncmp(buffer, "[Log]", 5U) == 0)
		  section = SECTION_LOG;
	  else if (::strncmp(buffer, "[DAPNET]", 8U) == 0)
		  section = SECTION_DAPNET;
	  else
        section = SECTION_NONE;

      continue;
    }

    char* key   = ::strtok(buffer, " \t=\r\n");
    if (key == NULL)
      continue;

    char* value = ::strtok(NULL, "\r\n");
	if (section == SECTION_GENERAL) {
		if (::strcmp(key, "Callsign") == 0) {
			for (unsigned int i = 0U; value[i] != '\0'; i++) {
				if (!::isspace(value[i]))
					m_callsign.insert(m_callsign.end(), 1, value[i]);
			}
		}
		else if (::strcmp(key, "WhiteList") == 0) {
			char* p = ::strtok(value, ",\r\n");
			while (p != NULL) {
				unsigned int ric = (unsigned int)::atoi(p);
				if (ric > 0U)
					m_whiteList.push_back(ric);
				p = ::strtok(NULL, ",\r\n");
			}
		} else if (::strcmp(key, "RptAddress") == 0)
			m_rptAddress = value;
		else if (::strcmp(key, "RptPort") == 0)
			m_rptPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "LocalAddress") == 0)
			m_myAddress = value;
		else if (::strcmp(key, "LocalPort") == 0)
			m_myPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Daemon") == 0)
			m_daemon = ::atoi(value) == 1;
	} else if (section == SECTION_LOG) {
		if (::strcmp(key, "FilePath") == 0)
			m_logFilePath = value;
		else if (::strcmp(key, "FileRoot") == 0)
			m_logFileRoot = value;
		else if (::strcmp(key, "FileLevel") == 0)
			m_logFileLevel = (unsigned int)::atoi(value);
		else if (::strcmp(key, "DisplayLevel") == 0)
			m_logDisplayLevel = (unsigned int)::atoi(value);
	} else if (section == SECTION_DAPNET) {
		if (::strcmp(key, "Address") == 0)
			m_dapnetAddress = value;
		else if (::strcmp(key, "Port") == 0)
			m_dapnetPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "AuthKey") == 0) {
			for (unsigned int i = 0U; value[i] != '\0'; i++) {
				if (!::isspace(value[i]))
					m_dapnetAuthKey.insert(m_dapnetAuthKey.end(), 1, value[i]);
			}
		}
		else if (::strcmp(key, "Debug") == 0)
			m_dapnetDebug = ::atoi(value) == 1;
	}
  }

  ::fclose(fp);

  return true;
}

std::string CConf::getCallsign() const
{
        return m_callsign;
}

std::vector<uint32_t> CConf::getWhiteList() const
{
	return m_whiteList;
}

std::string CConf::getRptAddress() const
{
	return m_rptAddress;
}

unsigned int CConf::getRptPort() const
{
	return m_rptPort;
}

std::string CConf::getMyAddress() const
{
	return m_myAddress;
}

unsigned int CConf::getMyPort() const
{
	return m_myPort;
}

bool CConf::getDaemon() const
{
	return m_daemon;
}

unsigned int CConf::getLogDisplayLevel() const
{
	return m_logDisplayLevel;
}

unsigned int CConf::getLogFileLevel() const
{
	return m_logFileLevel;
}

std::string CConf::getLogFilePath() const
{
        return m_logFilePath;
}

std::string CConf::getLogFileRoot() const
{
        return m_logFileRoot;
}

std::string CConf::getDAPNETAddress() const
{
	return m_dapnetAddress;
}

unsigned int CConf::getDAPNETPort() const
{
	return m_dapnetPort;
}

std::string CConf::getDAPNETAuthKey() const
{
	return m_dapnetAuthKey;
}

bool CConf::getDAPNETDebug() const
{
	return m_dapnetDebug;
}
