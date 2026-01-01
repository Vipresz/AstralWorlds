#pragma once

#include <iostream>
#include <fstream>
#include <streambuf>
#include <string>

// Helper class to redirect stdout/stderr to both console and log file
class ConsoleRedirect {
public:
    ConsoleRedirect(const std::string& logFilePath);
    ~ConsoleRedirect();
    
    bool IsValid() const { return m_valid; }

private:
    class TeeBuffer : public std::streambuf {
    public:
        TeeBuffer(std::streambuf* sb1, std::streambuf* sb2) 
            : m_sb1(sb1), m_sb2(sb2) {}
        
    protected:
        virtual int overflow(int c) {
            if (c != EOF) {
                char ch = static_cast<char>(c);
                if (m_sb1) m_sb1->sputc(ch);
                if (m_sb2) m_sb2->sputc(ch);
            }
            return c;
        }
        
        virtual int sync() {
            int r1 = m_sb1 ? m_sb1->pubsync() : 0;
            int r2 = m_sb2 ? m_sb2->pubsync() : 0;
            return (r1 == 0 && r2 == 0) ? 0 : -1;
        }
        
    private:
        std::streambuf* m_sb1;
        std::streambuf* m_sb2;
    };
    
    std::ofstream m_logFile;
    std::streambuf* m_originalCout;
    std::streambuf* m_originalCerr;
    TeeBuffer* m_coutBuffer;
    TeeBuffer* m_cerrBuffer;
    bool m_valid;
};


