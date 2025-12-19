#include "ConsoleRedirect.h"
#include <iostream>

ConsoleRedirect::ConsoleRedirect(const std::string& logFilePath)
    : m_originalCout(nullptr)
    , m_originalCerr(nullptr)
    , m_coutBuffer(nullptr)
    , m_cerrBuffer(nullptr)
    , m_valid(false)
{
    try {
        m_logFile.open(logFilePath, std::ios::out | std::ios::app);
        if (!m_logFile.is_open()) {
            std::cerr << "Failed to open log file for console redirect: " << logFilePath << std::endl;
            return;
        }
        
        m_coutBuffer = new TeeBuffer(std::cout.rdbuf(), m_logFile.rdbuf());
        m_cerrBuffer = new TeeBuffer(std::cerr.rdbuf(), m_logFile.rdbuf());
        
        m_originalCout = std::cout.rdbuf();
        m_originalCerr = std::cerr.rdbuf();
        
        std::cout.rdbuf(m_coutBuffer);
        std::cerr.rdbuf(m_cerrBuffer);
        
        m_valid = true;
        
        m_logFile << "\n=== CONSOLE OUTPUT REDIRECT STARTED ===\n";
        m_logFile.flush();
        
    } catch (...) {
        std::cerr << "Exception while setting up console redirect" << std::endl;
        m_valid = false;
    }
}

ConsoleRedirect::~ConsoleRedirect() {
    if (m_valid) {
        if (m_originalCout) {
            std::cout.rdbuf(m_originalCout);
        }
        if (m_originalCerr) {
            std::cerr.rdbuf(m_originalCerr);
        }
        
        if (m_logFile.is_open()) {
            m_logFile << "=== CONSOLE OUTPUT REDIRECT ENDED ===\n\n";
            m_logFile.flush();
        }
        
        delete m_coutBuffer;
        delete m_cerrBuffer;
        
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }
}


