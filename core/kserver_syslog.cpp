/// @file kserver_syslog.cpp
///
/// @brief Inplementation of kserver_syslog.hpp
///
/// @author Thomas Vanderbruggen <thomas@koheron.com>
/// @date 21/07/2015
///
/// (c) Koheron 2014-2015

#include "kserver_syslog.hpp"

#include <stdio.h>
#include <syslog.h>
#include <cstdarg>
#include <cstring>

#if KSERVER_HAS_THREADS
#  include <thread>
#endif

#include "kserver_defs.hpp"

namespace kserver {

SysLog::SysLog(KServerConfig *config_)
: config(config_)
{
    memset(fmt_buffer, 0, FMT_BUFF_LEN);

    if(config->syslog) {
        setlogmask(LOG_UPTO(KSERVER_SYSLOG_UPTO));
        openlog("KServer", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
    }
}

SysLog::~SysLog()
{}

// This cannot be done in the destructor
// since it is called after the "delete config"
// at the end of the main()
void SysLog::close()
{
    assert(config != NULL);

    if(config->syslog) {
        print(INFO, "Close syslog ...\n");
        closelog();
    }
}

int SysLog::print_stderr(const char *header, const char *message, ...)
{
    va_list argptr;
    va_start(argptr, message);
    
    int ret = snprintf(fmt_buffer, FMT_BUFF_LEN, "%s: %s", header, message);

    if(ret < 0) {
        fprintf(stderr, "Format error\n");
        return -1;
    }

    if(ret >= FMT_BUFF_LEN) {
        fprintf(stderr, "Buffer fmt_buffer overflow\n");
        return -1;
    }
    
    vfprintf(stderr, fmt_buffer, argptr);
    va_end(argptr);
    
    return 0;
}

void SysLog::print(unsigned int severity, const char *message, ...)
{    
#if KSERVER_HAS_THREADS
    std::lock_guard<std::mutex> lock(mutex);
#endif

    va_list argptr, argptr2;
    va_start(argptr, message);
    
    // See http://comments.gmane.org/gmane.linux.suse.programming-e/1107
    va_copy(argptr2, argptr);

    switch(severity) {
      case PANIC:
        print_stderr("KSERVER PANIC", message, argptr);
        
        if(config->syslog) {
            vsyslog(LOG_ALERT, message, argptr2);
        }
        
        break;
      case CRITICAL:
        print_stderr("KSERVER CRITICAL", message, argptr);
        
        if(config->syslog) {
            vsyslog(LOG_CRIT, message, argptr2);
        }

        break;
      case ERROR:
        print_stderr("KSERVER ERROR", message, argptr);
        
        if(config->syslog) {
            vsyslog(LOG_ERR, message, argptr2);
        }
        
        break;
      case WARNING:
        print_stderr("KSERVER WARNING", message, argptr);
        
        if(config->syslog) {
            vsyslog(LOG_WARNING, message, argptr2);
        }
        
        break;
      case INFO:
        if(config->verbose) {
            vprintf(message, argptr);
        }
        
        if(config->syslog) {
            vsyslog(LOG_NOTICE, message, argptr2);
        }
        break;
      case DEBUG:
        if(config->verbose) {
            vprintf(message, argptr);
        }
        
        if(config->syslog) {
            vsyslog(LOG_DEBUG, message, argptr2);
        }
    
        break;
      default:
        fprintf(stderr, "Invalid severity level\n");
    }
    
    va_end(argptr2);
    va_end(argptr);
}

} // namespace kserver

