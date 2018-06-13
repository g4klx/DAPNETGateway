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

#include "DAPNETGateway.h"
#include "StopWatch.h"
#include "Version.h"
#include "Thread.h"
#include "Timer.h"
#include "Log.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
const char* DEFAULT_INI_FILE = "DAPNETGateway.ini";
#else
const char* DEFAULT_INI_FILE = "/etc/DAPNETGateway.ini";
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <cassert>
#include <cmath>

const unsigned char FUNCTIONAL_NUMERIC      = 0U;
const unsigned char FUNCTIONAL_ALPHANUMERIC = 3U;

int main(int argc, char** argv)
{
	const char* iniFile = DEFAULT_INI_FILE;
	if (argc > 1) {
		for (int currentArg = 1; currentArg < argc; ++currentArg) {
			std::string arg = argv[currentArg];
			if ((arg == "-v") || (arg == "--version")) {
				::fprintf(stdout, "DAPNETGateway version %s\n", VERSION);
				return 0;
			} else if (arg.substr(0, 1) == "-") {
				::fprintf(stderr, "Usage: DAPNETGateway [-v|--version] [filename]\n");
				return 1;
			} else {
				iniFile = argv[currentArg];
			}
		}
	}

	CDAPNETGateway* gateway = new CDAPNETGateway(std::string(iniFile));

	int ret = gateway->run();

	delete gateway;

	return ret;
}

CDAPNETGateway::CDAPNETGateway(const std::string& configFile) :
m_conf(configFile),
m_dapnetNetwork(NULL),
m_pocsagNetwork(NULL),
m_queue()
{
}

CDAPNETGateway::~CDAPNETGateway()
{
	for (std::deque<CPOCSAGMessage*>::iterator it = m_queue.begin(); it != m_queue.end(); ++it)
		delete *it;

	m_queue.clear();
}

int CDAPNETGateway::run()
{
	bool ret = m_conf.read();
	if (!ret) {
		::fprintf(stderr, "DAPNETGateway: cannot read the .ini file\n");
		return 1;
	}

	setlocale(LC_ALL, "C");

	ret = ::LogInitialise(m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel());
	if (!ret) {
		::fprintf(stderr, "DAPNETGateway: unable to open the log file\n");
		return 1;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	bool m_daemon = m_conf.getDaemon();
	if (m_daemon) {
		// Create new process
		pid_t pid = ::fork();
		if (pid == -1) {
			::LogWarning("Couldn't fork() , exiting");
			return -1;
		}
		else if (pid != 0)
			exit(EXIT_SUCCESS);

		// Create new session and process group
		if (::setsid() == -1) {
			::LogWarning("Couldn't setsid(), exiting");
			return -1;
		}

		// Set the working directory to the root directory
		if (::chdir("/") == -1) {
			::LogWarning("Couldn't cd /, exiting");
			return -1;
		}

		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);

		//If we are currently root...
		if (getuid() == 0) {
			struct passwd* user = ::getpwnam("mmdvm");
			if (user == NULL) {
				::LogError("Could not get the mmdvm user, exiting");
				return -1;
			}

			uid_t mmdvm_uid = user->pw_uid;
			gid_t mmdvm_gid = user->pw_gid;

			//Set user and group ID's to mmdvm:mmdvm
			if (setgid(mmdvm_gid) != 0) {
				::LogWarning("Could not set mmdvm GID, exiting");
				return -1;
			}

			if (setuid(mmdvm_uid) != 0) {
				::LogWarning("Could not set mmdvm UID, exiting");
				return -1;
			}

			//Double check it worked (AKA Paranoia) 
			if (setuid(0) != -1) {
				::LogWarning("It's possible to regain root - something is wrong!, exiting");
				return -1;
			}
		}
	}
#endif

	bool debug             = m_conf.getDAPNETDebug();

	std::string rptAddress = m_conf.getRptAddress();
	unsigned int rptPort   = m_conf.getRptPort();
	std::string myAddress  = m_conf.getMyAddress();
	unsigned int myPort    = m_conf.getMyPort();

	m_pocsagNetwork = new CPOCSAGNetwork(myAddress, myPort, rptAddress, rptPort, debug);
	ret = m_pocsagNetwork->open();
	if (!ret) {
		::LogError("Cannot open the repeater network port");
		::LogFinalise();

		return 1;
	}

	std::string callsign      = m_conf.getCallsign();
	std::string dapnetAddress = m_conf.getDAPNETAddress();
	unsigned int dapnetPort   = m_conf.getDAPNETPort();
	std::string dapnetAuthKey = m_conf.getDAPNETAuthKey();

	m_dapnetNetwork = new CDAPNETNetwork(dapnetAddress, dapnetPort, callsign, dapnetAuthKey, VERSION, debug);
	ret = m_dapnetNetwork->open();
	if (!ret) {
		m_pocsagNetwork->close();
		delete m_pocsagNetwork;
		delete m_dapnetNetwork;

		::LogError("Cannot open the DAPNET network port");
		::LogFinalise();

		return 1;
	}

	ret = m_dapnetNetwork->login();
	if (!ret) {
		m_pocsagNetwork->close();
		m_dapnetNetwork->close();
		delete m_pocsagNetwork;
		delete m_dapnetNetwork;

		::LogError("Cannot login to the DAPNET network");
		::LogFinalise();

		return 1;
	}

	::LogMessage("Logged into the DAPNET network");

	CTimer messageTimer(1000U, 0U, 200U);

	CStopWatch stopWatch;
	stopWatch.start();

	LogMessage("Starting DAPNETGateway-%s", VERSION);

	for (;;) {
		unsigned char buffer[200U];

		if (m_pocsagNetwork->read(buffer) > 0U) {
			switch (buffer[0U]) {
			case 0x00U:
				// The MMDVM is idle
				messageTimer.start();
				break;
			case 0xFFU:
				// The MMDVM is busy
				messageTimer.stop();
				break;
			default:
				// The MMDVM is sending crap
				LogWarning("Unknown data from the MMDVM - 0x%02X", buffer[0U]);
				break;
			}
		}

		bool ok = m_dapnetNetwork->read();
		if (!ok)
			recover();

		CPOCSAGMessage* message = m_dapnetNetwork->readMessage();
		if (message != NULL) {
			if (!messageTimer.isRunning() || !m_queue.empty()) {
				if (!isTimeMessage(message)) {
					LogDebug("Queueing message to %07u, type %u, func %s: \"%.*s\"", message->m_ric, message->m_type, message->m_functional == FUNCTIONAL_NUMERIC ? "Numeric" : "Alphanumeric", message->m_length, message->m_message);
					m_queue.push_front(message);
				} else {
					LogDebug("Rejecting message to %07u, type %u, func %s: \"%.*s\"", message->m_ric, message->m_type, message->m_functional == FUNCTIONAL_NUMERIC ? "Numeric" : "Alphanumeric", message->m_length, message->m_message);
				}
			} else {
				LogMessage("Sending message to %07u, type %u, func %s: \"%.*s\"", message->m_ric, message->m_type, message->m_functional == FUNCTIONAL_NUMERIC ? "Numeric" : "Alphanumeric", message->m_length, message->m_message);
			}
		}

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		messageTimer.clock(ms);
		if (messageTimer.isRunning() && messageTimer.hasExpired()) {
			if (!m_queue.empty())
				sendData();

			messageTimer.start();
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	m_pocsagNetwork->close();
	delete m_pocsagNetwork;

	m_dapnetNetwork->close();
	delete m_dapnetNetwork;

	::LogFinalise();

	return 0;
}

bool CDAPNETGateway::sendData()
{
	assert(m_pocsagNetwork != NULL);

	if (!m_queue.empty()) {
		CPOCSAGMessage* message = m_queue.back();
		m_queue.pop_back();
		LogMessage("Sending message to %07u, type %u, func %s: \"%.*s\"", message->m_ric, message->m_type, message->m_functional == FUNCTIONAL_NUMERIC ? "Numeric" : "Alphanumeric", message->m_length, message->m_message);
		m_pocsagNetwork->write(message);
		delete message;
		return true;
	}

	return false;
}

bool CDAPNETGateway::recover()
{
	for (;;) {
		m_dapnetNetwork->close();
		bool ok = m_dapnetNetwork->open();
		if (ok) {
			ok = m_dapnetNetwork->login();
			if (ok)
				return true;
		}
	}

	return false;
}

bool CDAPNETGateway::isTimeMessage(const CPOCSAGMessage* message) const
{
	if (message->m_type == 5U && message->m_functional == FUNCTIONAL_NUMERIC)
		return true;

	if (message->m_type == 6U && message->m_functional == FUNCTIONAL_ALPHANUMERIC && ::memcmp(message->m_message, "XTIME=", 6U) == 0)
		return true;

	return false;
}

