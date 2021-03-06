/// @file signal_handler.cpp
///
/// @brief Implementation of signal_handler.hpp
///
/// @author Thomas Vanderbruggen <thomas@koheron.com>
/// @date 11/09/2015
///
/// (c) Koheron 2014-2015

#include "signal_handler.hpp"

#include <iostream>
#include <cxxabi.h>

extern "C" { 
  #include <signal.h>
  #include <execinfo.h>
}

#include "kserver.hpp"

namespace kserver {

KServer* SignalHandler::kserver = nullptr;

SignalHandler::SignalHandler()
{}

SignalHandler::~SignalHandler()
{
    // TODO Restore disabled signals
}

int SignalHandler::Init(KServer *kserver_)
{
    kserver = kserver_;

    if(set_interrup_signals() < 0 || 
       set_ignore_signals()   < 0 ||
       set_crash_signals()    < 0)
        return -1;
        
    return 0;
}

// Interrupt signals

int volatile SignalHandler::s_interrupted = 0;

void exit_signal_handler(int s)
{
    SignalHandler::s_interrupted = 1;
}

int SignalHandler::set_interrup_signals()
{
    struct sigaction sig_int_handler;

    sig_int_handler.sa_handler = exit_signal_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;

    if(sigaction(SIGINT, &sig_int_handler, NULL) < 0) {
        kserver->syslog.print(SysLog::CRITICAL, "Cannot set SIGINT handler\n");
        return -1;
    }
    
    if(sigaction(SIGTERM, &sig_int_handler, NULL) < 0) {
        kserver->syslog.print(SysLog::CRITICAL, "Cannot set SIGTERM handler\n");
        return -1;
    }

    return 0;
}

// Ignored signals

int SignalHandler::set_ignore_signals()
{
    struct sigaction sig_ign_handler;
    
    sig_ign_handler.sa_handler = SIG_IGN;
    sig_ign_handler.sa_flags = 0;
    sigemptyset(&sig_ign_handler.sa_mask);
    
    // Disable SIGPIPE which is call by the socket write function
    // when client closes its connection during writing.
    // Results in an unwanted server shutdown
    if(sigaction(SIGPIPE, &sig_ign_handler, 0) < 0) {
        kserver->syslog.print(SysLog::CRITICAL, "Cannot disable SIGPIPE\n");
        return -1;
    }
    
    // XXX TV
    // According to this:
    // http://stackoverflow.com/questions/7296923/different-signal-handler-for-thread-and-process-is-it-possible
    //
    // SIGPIPE is delivered to the thread generating it. 
    // It might thus be possible to stop the session emitting it.
    
    if(sigaction(SIGTSTP, &sig_ign_handler, 0) < 0) {
        kserver->syslog.print(SysLog::CRITICAL, "Cannot disable SIGTSTP\n");
        return -1;
    }
    
    return 0;
}

// Crashed signals
// 
// Write backtrace to the syslog. See:
// http://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes

#define BACKTRACE_BUFF_SIZE 100

void crash_signal_handler(int sig)
{
    // The signal handler is called several times 
    // on a segmentation fault (WHY ?).
    // So only display the backtrace the first time
    if(SignalHandler::s_interrupted)
        return;
        
    const char *sig_name;
    
    switch(sig) {
      case SIGBUS:
        sig_name = "(Bus Error)";
        break;
      case SIGSEGV:
        sig_name = "(Segmentation Fault)";
        break;
      default:
        sig_name = "(Unidentify signal)";
    }

    SignalHandler::kserver->syslog.print(SysLog::CRITICAL, 
                              "CRASH: signal %d %s\n", sig, sig_name);

    void *buffer[BACKTRACE_BUFF_SIZE];
    size_t size = backtrace(buffer, BACKTRACE_BUFF_SIZE);             
    char **messages = backtrace_symbols(buffer, size);
    
    if (messages == NULL) {
        SignalHandler::kserver->syslog.print(SysLog::ERROR, 
                                             "No backtrace_symbols");
        goto exit;
    }
    
    for(unsigned int i = 0; i < size && messages != NULL; i++) {
        char *mangled_name = 0, *offset_begin = 0, *offset_end = 0;

        // Find parantheses and +address offset surrounding mangled name
        for(char *p = messages[i]; *p; ++p) {
            if(*p == '(') {
                mangled_name = p; 
            }
            else if(*p == '+') {
                offset_begin = p;
            }
            else if(*p == ')') {
                offset_end = p;
                break;
            }
        }

        // If the line could be processed, attempt to demangle the symbol
        if(mangled_name && offset_begin && offset_end
           && mangled_name < offset_begin) {
            *mangled_name++ = '\0';
            *offset_begin++ = '\0';
            *offset_end++ = '\0';

            int status;
            char *real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

            // If demangling is successful, output the demangled function name
            if(status == 0) {  
                SignalHandler::kserver->syslog.print(SysLog::INFO, 
                        "[bt]: (%d) %s : %s+%s%s\n", 
                        i, messages[i], real_name, offset_begin, offset_end);
            } else { // Otherwise, output the mangled function name
                SignalHandler::kserver->syslog.print(SysLog::INFO, 
                        "[bt]: (%d) %s : %s+%s%s\n", 
                        i, messages[i], mangled_name, offset_begin, offset_end);
            }
            
            free(real_name);
        }
    }
        
    free(messages);
    
exit:
    // Exit KServer
    SignalHandler::s_interrupted = 1;
}

int SignalHandler::set_crash_signals()
{
    struct sigaction sig_crash_handler;
    
    sig_crash_handler.sa_handler = crash_signal_handler;
    sigemptyset(&sig_crash_handler.sa_mask);
    sig_crash_handler.sa_flags = SA_RESTART | SA_SIGINFO;

    if(sigaction(SIGSEGV, &sig_crash_handler, NULL) < 0) {
        kserver->syslog.print(SysLog::CRITICAL, "Cannot set SIGSEGV handler\n");
        return -1;
    }
    
    if(sigaction(SIGBUS, &sig_crash_handler, NULL) < 0) {
        kserver->syslog.print(SysLog::CRITICAL, "Cannot set SIGBUSs handler\n");
        return -1;
    }

    return 0;
}

} // namespace kserver
