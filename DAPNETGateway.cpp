/*
*   Copyright (C) 2018,2020,2024,2025 by Jonathan Naylor G4KLX
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
#include "GitVersion.h"

#include "REGEX.h"
#include <regex>

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

static bool m_killed = false;
static int  m_signal = 0;

#if !defined(_WIN32) && !defined(_WIN64)
static void sigHandler(int signum)
{
	m_killed = true;
	m_signal = signum;
}
#endif

#include <algorithm>
#include <utility>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <cassert>
#include <cmath>

const unsigned int FRAME_LENGTH_CODEWORDS    = 2U;
const unsigned int BATCH_LENGTH_CODEWORDS    = 17U;
const unsigned int PREAMBLE_LENGTH_CODEWORDS = BATCH_LENGTH_CODEWORDS + 1U;

const unsigned int CODEWORD_TIME_US = 26667U;										// 26.667ms
const unsigned int FRAME_TIME_US = CODEWORD_TIME_US * FRAME_LENGTH_CODEWORDS;		// 53.333ms
const unsigned int BATCH_TIME_US = CODEWORD_TIME_US * BATCH_LENGTH_CODEWORDS;		// 453.333ms

const unsigned int PREAMBLE_TIME_US = CODEWORD_TIME_US * PREAMBLE_LENGTH_CODEWORDS;	// 480.006ms

const unsigned int SLOT_TIME_US       = 6400000U;									// 6.4s
const unsigned int SLOT_TIME_MS       = SLOT_TIME_US / 1000U;						// 6.4s
const unsigned int CODEWORDS_PER_SLOT = SLOT_TIME_US / CODEWORD_TIME_US;			// 240
const unsigned int BATCHES_PER_SLOT   = SLOT_TIME_US / BATCH_TIME_US;				// 14

const unsigned char FUNCTIONAL_NUMERIC      = 0U;
const unsigned char FUNCTIONAL_ALERT1       = 1U;
const unsigned char FUNCTIONAL_ALERT2       = 2U;
const unsigned char FUNCTIONAL_ALPHANUMERIC = 3U;

const unsigned int MAX_TIME_TO_HOLD_TIME_MESSAGES = 15000U;		// 15s

int main(int argc, char** argv)
{
	const char* iniFile = DEFAULT_INI_FILE;
	if (argc > 1) {
		for (int currentArg = 1; currentArg < argc; ++currentArg) {
			std::string arg = argv[currentArg];
			if ((arg == "-v") || (arg == "--version")) {
				::fprintf(stdout, "DAPNETGateway version %s git #%.7s\n", VERSION, gitversion);
				return 0;
			} else if (arg.substr(0, 1) == "-") {
				::fprintf(stderr, "Usage: DAPNETGateway [-v|--version] [filename]\n");
				return 1;
			} else {
				iniFile = argv[currentArg];
			}
		}
	}

#if !defined(_WIN32) && !defined(_WIN64)
	::signal(SIGINT,  sigHandler);
	::signal(SIGTERM, sigHandler);
	::signal(SIGHUP,  sigHandler);
#endif

	int ret = 0;

	do {
		m_signal = 0;
		m_killed = false;

		CDAPNETGateway* gateway = new CDAPNETGateway(std::string(iniFile));
		ret = gateway->run();

		delete gateway;

		switch (m_signal) {
			case 0:
				break;
			case 2:
				::LogInfo("DAPNETGateway-%s exited on receipt of SIGINT", VERSION);
				break;
			case 15:
				::LogInfo("DAPNETGateway-%s exited on receipt of SIGTERM", VERSION);
				break;
			case 1:
				::LogInfo("DAPNETGateway-%s is restarting on receipt of SIGHUP", VERSION);
				break;
			default:
				::LogInfo("DAPNETGateway-%s exited on receipt of an unknown signal", VERSION);
				break;
		}
	} while (m_signal == 1);

	::LogFinalise();

	return ret;
}

CDAPNETGateway::CDAPNETGateway(const std::string& configFile) :
m_conf(configFile),
m_dapnetNetwork(nullptr),
m_pocsagNetwork(nullptr),
m_queue(),
m_slotTimer(),
m_schedule(nullptr),
m_allSlots(false),
m_currentSlot(0U),
m_sentCodewords(0U),
m_regexBlacklist(),
m_regexWhitelist(),
m_mmdvmFree(false)
{
	CUDPSocket::startup();
}

CDAPNETGateway::~CDAPNETGateway()
{
	for (std::deque<CPOCSAGMessage*>::iterator it = m_queue.begin(); it != m_queue.end(); ++it)
		delete *it;

	m_queue.clear();

	CUDPSocket::shutdown();
}

int CDAPNETGateway::run()
{
	bool ret = m_conf.read();
	if (!ret) {
		::fprintf(stderr, "DAPNETGateway: cannot read the .ini file\n");
		return 1;
	}

	setlocale(LC_ALL, "C");

#if !defined(_WIN32) && !defined(_WIN64)
	bool m_daemon = m_conf.getDaemon();
	if (m_daemon) {
		// Create new process
		pid_t pid = ::fork();
		if (pid == -1) {
			::fprintf(stderr, "Couldn't fork() , exiting\n");
			return 1;
		} else if (pid != 0) {
			exit(EXIT_SUCCESS);
		}

		// Create new session and process group
		if (::setsid() == -1) {
			::fprintf(stderr, "Couldn't setsid(), exiting\n");
			return 1;
		}

		// Set the working directory to the root directory
		if (::chdir("/") == -1) {
			::fprintf(stderr, "Couldn't cd /, exiting\n");
			return 1;
		}

		// If we are currently root...
		if (getuid() == 0) {
			struct passwd* user = ::getpwnam("mmdvm");
			if (user == nullptr) {
				::fprintf(stderr, "Could not get the mmdvm user, exiting\n");
				return 1;
			}

			uid_t mmdvm_uid = user->pw_uid;
			gid_t mmdvm_gid = user->pw_gid;

			// Set user and group ID's to mmdvm:mmdvm
			if (setgid(mmdvm_gid) != 0) {
				::fprintf(stderr, "Could not set mmdvm GID, exiting\n");
				return 1;
			}

			if (setuid(mmdvm_uid) != 0) {
				::fprintf(stderr, "Could not set mmdvm UID, exiting\n");
				return 1;
			}

			// Double check it worked (AKA Paranoia) 
			if (setuid(0) != -1) {
				::fprintf(stderr, "It's possible to regain root - something is wrong!, exiting\n");
				return 1;
			}
		}
	}
#endif

#if !defined(_WIN32) && !defined(_WIN64)
        ret = ::LogInitialise(m_daemon, m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel(), m_conf.getLogFileRotate());
#else
        ret = ::LogInitialise(false, m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel(), m_conf.getLogFileRotate());
#endif
	if (!ret) {
		::fprintf(stderr, "DAPNETGateway: unable to open the log file\n");
		return 1;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	if (m_daemon) {
		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
	}
#endif

	bool debug             = m_conf.getDAPNETDebug();

	std::string rptAddress = m_conf.getRptAddress();
	unsigned short rptPort = m_conf.getRptPort();
	std::string myAddress  = m_conf.getMyAddress();
	unsigned short myPort  = m_conf.getMyPort();

	m_pocsagNetwork = new CPOCSAGNetwork(myAddress, myPort, rptAddress, rptPort, debug);
	ret = m_pocsagNetwork->open();
	if (!ret) {
		::LogError("Cannot open the repeater network port");
		return 1;
	}

	std::string callsign      = m_conf.getCallsign();
	std::string dapnetAddress = m_conf.getDAPNETAddress();
	unsigned short dapnetPort = m_conf.getDAPNETPort();
	std::string dapnetAuthKey = m_conf.getDAPNETAuthKey();

	if (dapnetAuthKey.length() == 0 || dapnetAuthKey == "TOPSECRET") {
		::LogError("AuthKey not set or invalid");
		return 1;
	}
		
	m_dapnetNetwork = new CDAPNETNetwork(dapnetAddress, dapnetPort, callsign, dapnetAuthKey, VERSION, false, 1, debug);
	ret = m_dapnetNetwork->open();
	if (!ret) {
		m_pocsagNetwork->close();
		delete m_pocsagNetwork;
		delete m_dapnetNetwork;

		::LogError("Cannot open the DAPNET network port");

		return 1;
	}

	LogMessage("Starting DAPNETGateway-%s", VERSION);

	ret = m_dapnetNetwork->login();
	if (!ret) {
		m_pocsagNetwork->close();
		m_dapnetNetwork->close();
		delete m_pocsagNetwork;
		delete m_dapnetNetwork;

		::LogError("Cannot login to the DAPNET network");

		return 1;
	}

	std::vector<unsigned int> whiteList = m_conf.getWhiteList();
	std::vector<unsigned int> blackList = m_conf.getBlackList();

	std::vector<std::regex> regexBlacklist;
	std::vector<std::regex> regexWhitelist;

	LogMessage("Initializing blacklist");
	m_regexBlacklist = new CREGEX(m_conf.getblacklistRegexfile());
	if (m_regexBlacklist->load()) 
		regexBlacklist = m_regexBlacklist->get();

	LogMessage("Initializing whitelist");
	m_regexWhitelist = new CREGEX(m_conf.getwhitelistRegexfile());
		if (m_regexWhitelist->load())
		regexWhitelist = m_regexWhitelist->get();


	while (!m_killed) {
		unsigned char buffer[200U];

		if (m_pocsagNetwork->read(buffer) > 0U) {
			switch (buffer[0U]) {
			case 0x00U:
				// The MMDVM is idle
				if (!m_mmdvmFree) {
					// LogDebug("*** MMDVM is free");
					m_mmdvmFree = true;
					m_sentCodewords = (m_slotTimer.elapsed() * 1000U) / CODEWORD_TIME_US;
				}
				break;
			case 0xFFU:
				// The MMDVM is busy
				// LogDebug("*** MMDVM is busy");
				m_mmdvmFree = false;
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
		if (message != nullptr) {
			bool found = true;
			bool blackListRIC = false;
			bool blacklistRegexmatch = false;
			bool whitelistRegexmatch = true;

			// If we have a white list of RICs, use it.
			if (!whiteList.empty())
				found = std::find(whiteList.begin(), whiteList.end(), message->m_ric) != whiteList.end();

			// If we have a black list of RICs, use it.
			if (!blackList.empty())
				blackListRIC = std::find(blackList.begin(), blackList.end(), message->m_ric) != blackList.end();
			if (blackListRIC)
				LogDebug("Blacklist match: Not queueing message to %07u, type %u, message: \"%.*s\"", message->m_ric, message->m_type, message->m_length, message->m_message);

			std::string  messageBody(reinterpret_cast<char*>(message->m_message));
			//If we have a list of blacklist REGEXes, use them 
			if (!regexBlacklist.empty()) {
				for (std::regex regex : regexBlacklist) {
					bool ret =  std::regex_match(messageBody,regex);
					//If the regex matches the message body, don't send the message
					if (ret) {
						blacklistRegexmatch = true;
						LogDebug("Blacklist REGEX match: Not queueing message to %07u, type %u, message: \"%.*s\"", message->m_ric, message->m_type, message->m_length, messageBody.c_str());
					}
				}
			}

			if(!regexWhitelist.empty() && !blacklistRegexmatch) {
				for (std::regex regex : regexWhitelist) {
					bool ret =  std::regex_match(messageBody,regex);
					//If the regex does not match the message body, don't send the message
					if (!ret) {
						whitelistRegexmatch = false;
						LogDebug("No whitelist REGEX match: Not queueing message to %07u, type %u, message: \"%.*s\"", message->m_ric, message->m_type, message->m_length, messageBody.c_str());
					}
				}
			}

			if (found && !blackListRIC && !blacklistRegexmatch && whitelistRegexmatch) {
				switch (message->m_functional) {
					case FUNCTIONAL_ALPHANUMERIC:
						LogDebug("Queueing message to %07u, type %u, func Alphanumeric: \"%.*s\"", message->m_ric, message->m_type, message->m_length, message->m_message);
						break;
					case FUNCTIONAL_ALERT2:
						LogDebug("Queueing message to %07u, type %u, func Alert 2: \"%.*s\"", message->m_ric, message->m_type, message->m_length, message->m_message);
						break;
					case FUNCTIONAL_NUMERIC:
						LogDebug("Queueing message to %07u, type %u, func Numeric: \"%.*s\"", message->m_ric, message->m_type, message->m_length, message->m_message);
						break;
					case FUNCTIONAL_ALERT1:
						LogDebug("Queueing message to %07u, type %u, func Alert 1", message->m_ric, message->m_type);
						break;
					default:
						break;
				}

				m_queue.push_front(message);
				LogDebug("Messages in Queue %04u", m_queue.size());
			}
		}

		unsigned int t = (m_slotTimer.time() / 100ULL) % 1024ULL;
		unsigned int slot = t / 64U;
		if (slot != m_currentSlot) {
			// LogDebug("Start of slot %u", slot);
			m_currentSlot = slot;
			if (m_schedule == nullptr || m_currentSlot == 0U)
				loadSchedule();
			m_sentCodewords = 0U;
			m_slotTimer.start();
		}

		sendMessages();

		CThread::sleep(10U);
	}

	m_pocsagNetwork->close();
	delete m_pocsagNetwork;

	m_dapnetNetwork->close();
	delete m_dapnetNetwork;

	return 0;
}

void CDAPNETGateway::sendMessages()
{
	// If the MMDVM is busy, we can't send anything.
	if (!m_mmdvmFree)
		return;

	// Do we have a schedule?
	if (m_schedule == nullptr)
		return;

	// Check to see if we're allowed to send within a slot.
	if (!m_schedule[m_currentSlot])
		return;

	// If we have data to send, see if we have time to do so in the current schedule.
	if (m_queue.empty())
		return;

	CPOCSAGMessage* message = m_queue.back();
	assert(message != nullptr);

	// Special case, only test if slots are being used.
	if (m_allSlots) {
		sendMessage(message);
		m_queue.pop_back();
		delete message;
		return;
	}

	unsigned int codewords = calculateCodewords(message);

	// Do we have too much data already sent in this slot?
	unsigned int totalCodewords = m_sentCodewords + PREAMBLE_LENGTH_CODEWORDS + codewords;
	if (totalCodewords >= CODEWORDS_PER_SLOT) {
		// LogDebug("Too many codewords sent in slot %u already %u + %u + %u = %u >= %u", m_currentSlot, m_sentCodewords, PREAMBLE_LENGTH_CODEWORDS, codewords, totalCodewords, CODEWORDS_PER_SLOT);
		return;
	}

	// Is there enough time to send it in this slot before it ends?
	unsigned int sendTime = (PREAMBLE_TIME_US + codewords * CODEWORD_TIME_US) / 1000U;
	unsigned int timeLeft = SLOT_TIME_MS - m_slotTimer.elapsed();
	if (sendTime >= timeLeft) {
		// LogDebug("Too little time to send the message in slot %u, %u + %u + %u = %u >= %u = %u - %u", m_currentSlot, PREAMBLE_TIME_US, codewords, CODEWORD_TIME_US, sendTime, timeLeft, SLOT_TIME_MS, m_slotTimer.elapsed());
		return;
	}

	bool ret = sendMessage(message);
	if (ret)
		m_sentCodewords = totalCodewords;

	m_queue.pop_back();
	delete message;
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

unsigned int CDAPNETGateway::calculateCodewords(const CPOCSAGMessage* message) const
{
	assert(message != nullptr);

	unsigned int len = 0U;
	switch (message->m_functional) {
		case FUNCTIONAL_NUMERIC:
			len = message->m_length / 5U;			// For the number packing, five to a word
			break;
		case FUNCTIONAL_ALPHANUMERIC:
		case FUNCTIONAL_ALERT2:
			len = (message->m_length * 7U) / 20U;	// For the ASCII packing, 7/20 to a word
			break;
		case FUNCTIONAL_ALERT1:
		default:
			break;
	}

	len++;										// For the address word

	if ((len % 2U) == 1U)						// Always an even number of words
		len++;

	len += len % 16U;							// A very long message will include sync words

	return len;
}

void CDAPNETGateway::loadSchedule()
{
	bool* schedule = m_dapnetNetwork->readSchedule();
	if (schedule == nullptr)
		return;

	delete[] m_schedule;
	m_schedule = schedule;

	m_allSlots = true;

	std::string text;
	for (unsigned int i = 0U; i < 16U; i++) {
		if (m_schedule[i]) {
			text += "*";
		} else {
			text += "-";
			m_allSlots = false;
		}
	}

	if (m_allSlots)
		LogMessage("All slots are available for transmission");
	else
		LogMessage("Loaded new schedule: %s", text.c_str());
}

bool CDAPNETGateway::sendMessage(CPOCSAGMessage* message) const
{
	assert(message != nullptr);

	bool ret = isTimeMessage(message);
	if (ret && message->m_timeQueued.elapsed() >= MAX_TIME_TO_HOLD_TIME_MESSAGES) {
		switch (message->m_functional) {
			case FUNCTIONAL_ALPHANUMERIC:
				LogDebug("Rejecting message to %07u, type %u, func Alphanumeric: \"%.*s\"", message->m_ric, message->m_type, message->m_length, message->m_message);
				break;
			case FUNCTIONAL_ALERT2:
				LogDebug("Rejecting message to %07u, type %u, func Alert 2: \"%.*s\"", message->m_ric, message->m_type, message->m_length, message->m_message);
				break;
			case FUNCTIONAL_NUMERIC:
				LogDebug("Rejecting message to %07u, type %u, func Numeric: \"%.*s\"", message->m_ric, message->m_type, message->m_length, message->m_message);
				break;
			case FUNCTIONAL_ALERT1:
				LogDebug("Rejecting message to %07u, type %u, func Alert 1", message->m_ric, message->m_type);
				break;
			default:
				break;
		}

		return false;
	} else {
		switch (message->m_functional) {
			case FUNCTIONAL_ALPHANUMERIC:
				LogMessage("Sending message in slot %u to %07u, type %u, func Alphanumeric: \"%.*s\"", m_currentSlot, message->m_ric, message->m_type, message->m_length, message->m_message);
				break;
			case FUNCTIONAL_ALERT2:
				LogMessage("Sending message in slot %u to %07u, type %u, func Alert 2: \"%.*s\"", m_currentSlot, message->m_ric, message->m_type, message->m_length, message->m_message);
				break;
			case FUNCTIONAL_NUMERIC:
				LogMessage("Sending message in slot %u to %07u, type %u, func Numeric: \"%.*s\"", m_currentSlot, message->m_ric, message->m_type, message->m_length, message->m_message);
				break;
			case FUNCTIONAL_ALERT1:
				LogMessage("Sending message in slot %u to %07u, type %u, func Alert 1", m_currentSlot, message->m_ric, message->m_type);
				break;
			default:
				break;
		}

		m_pocsagNetwork->write(message);
		return true;
	}
}
