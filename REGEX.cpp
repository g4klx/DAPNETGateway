/*
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
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

#include "REGEX.h"
#include "Log.h"

#include <cstdio>
#include <cstring>

CREGEX::CREGEX(const std::string& regexFile) :
m_regexFile(regexFile),
m_regex()
{
}

bool CREGEX::load()
{
	FILE* fp = ::fopen(m_regexFile.c_str(), "rt");
	if (fp != NULL) {
		char buffer[100U];
		std::regex regex;
		while (::fgets(buffer, 100U, fp) != NULL) {
			if (buffer[0U] == '#')
				continue;

			const char* regexStr = ::strtok(buffer, "\r\n");
			
			if (regexStr != NULL) {
				try {
					regex = std::regex(regexStr);
				}

				catch(const std::regex_error& e) {
					LogDebug("error in regex %s (%s), skipping",regexStr,e.what());
					continue;
				}
				m_regex.push_back(regex);
			}
		}

		::fclose(fp);
	}

	size_t size = m_regex.size();
	LogInfo("Loaded %u REGEX from file %s", size, regexFile.c_str());

	size = m_regex.size();
	if (size == 0U)
		return false;

	return true;
}

std::vector<std::regex>  CREGEX::get()
{
	return(m_regex);
}
