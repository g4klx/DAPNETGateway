/*
*   Copyright (C) 2016,2017,2025 by Jonathan Naylor G4KLX
*
*   Contributed by Simon G7RZU
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
/* Older versions of GCC appear to support REGEX but silently fail to match. Even
 * though the headers exist and the code compiles and runs cleanly. The below #if block 
 * stops the REGEXs being loaded in these cases, effectively disabling REGEX functionality. 
 * The loading code is replaced with a log entry to show the user this has happened.
*/
{
#if defined(__GNUC__) && (__GNUC__ <= 4) || (__GNUC__ == 4 && __GNUC_MINOR__ <= 9)
	LogError("REGEX is not properly supported in GCC versions below 4.9. Not loading REGEX");
	return false;
}
#else

	
	FILE* fp = ::fopen(m_regexFile.c_str(), "rt");
	if (fp != nullptr) {
		char buffer[100U];
		std::regex regex;
		while (::fgets(buffer, 100U, fp) != nullptr) {
			if (buffer[0U] == '#')
				continue;

			const char* regexStr = ::strtok(buffer, "\r\n");
			
			if (regexStr != nullptr) {
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
	LogInfo("Loaded %u REGEX from file %s", size, m_regexFile.c_str());

	size = m_regex.size();
	if (size == 0U)
		return false;

	return true;
}

#endif

std::vector<std::regex>  CREGEX::get()
{
	return(m_regex);
}
