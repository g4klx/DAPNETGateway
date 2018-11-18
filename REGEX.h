/*
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
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

#if !defined(REGEX_H)
#define	REGEX_H


#include <vector>
#include <string>
#include <regex>

class CREGEX {
public:

	std::vector<std::regex> get();
	bool load();
	CREGEX(const std::string& regexFile);
	~CREGEX();

private:
	
	std::string              m_regexFile;
	std::vector<std::regex>  m_regex;
};

#endif
