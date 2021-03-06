/// @file kdevice.hpp
///
/// @brief KServer devices
///
/// @author Thomas Vanderbruggen <thomas@koheron.com>
/// @date 22/11/2014
///
/// (c) Koheron 2014

#ifndef __KDEVICE_HPP__
#define __KDEVICE_HPP__

#include <cstring> 

#include "kserver_defs.hpp"
#include "dev_definitions.hpp"

namespace kserver {

// ---------------------------------------------
// KDevice
// ---------------------------------------------

template<class Dev, device_t dev_kind> class KDevice;

// X Macro: Device class list
#define EXPAND_AS_CLASS_LIST(num, name, operations ...)     \
        class name;

class KServer;
DEVICES_TABLE(EXPAND_AS_CLASS_LIST) // X-macro

class SessionManager;

/// @brief Abstract class for KDevice
class KDeviceAbstract {
public:
	KDeviceAbstract(device_t kind_)
	:kind(kind_) {}

	device_t kind = NO_DEVICE;

	template<class Dev, device_t dev_kind>
	KDevice<Dev, dev_kind>* cast() {
	    if(Dev::__kind != kind)
	        return NULL;
	    else
            return static_cast<KDevice<Dev, dev_kind>*>(this);
    }
      
    bool is_failed(void);
};

struct Command;
class KServerSession;

/// @brief Polymorph class for a KServer device
/// 
/// Uses static polymorphism with Curiously Recurring Template Pattern (CRTP)
/// http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern 
template<class Dev, device_t dev_kind>
class KDevice : public KDeviceAbstract
{
  public:
    /// Contains the arguments of the operation op
    template <int op> struct Argument;

	KDevice(KServer *kserver_)
	: KDeviceAbstract(dev_kind),
	  kserver(kserver_)
	{}

    /// @brief Execute the command
    int execute(const Command& cmd);
    
    /// @brief True if the device failed
    bool is_failed(void);
    
  private:
    /// Each device knows the KServer class,
    /// which itself knows every body else
    KServer* kserver;
    
    // TODO We should add this here for parsing instead of
    // initializing the array in each parsing function.
    // But got warning that args 'may be used uninitialized in this function'.
    // --> Need to investigate ...
    //char tmp_str[2*KSERVER_READ_STR_LEN];

  protected:
    /// @brief Parse the buffer of a command
    /// @cmd The Command to be parsed
    /// @args The arguments resulting of the parsing
    template<int op>
    int parse_arg(const Command& cmd, Argument<op>& args);

    /// @brief Execute an operation
    /// @args The arguments of the operation provided by @parse_arg
    /// @sess_id ID of the session executing the operation
    template<int op>
    int execute_op(const Argument<op>& args, SessID sess_id);
	
friend class DeviceManager;
friend Dev;
};

/// Macros to simplify edition of operations 
#define VERBOSE kserver->session_manager.GetSession(sess_id).GetParams().Verbose()
#define SEND kserver->session_manager.GetSession(sess_id).Send
#define SEND_ARRAY kserver->session_manager.GetSession(sess_id).SendArray
#define SEND_CSTR kserver->session_manager.GetSession(sess_id).SendCstr
#define RCV_HANDSHAKE kserver->session_manager.GetSession(sess_id).RcvHandshake

// Example of Device implementation
#ifdef NE_PAS_DEFINIR_CETTE_MACRO

// CRTP: Curiously Recurring Template Pattern
// http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern 
class MyDev :public KDevice<MyDev> 
{
public:
    enum { __kind = MY_TAG };

public:
    enum Operation {
        OP1,
        ops_num	
    };
};

template<>
int KDevice<MyDev>::execute(const Command & cmd) {

}

template<>
bool KDevice<MyDev>::is_failed(void) {

}

template<>
template<>
struct KDevice<MyDev>::Argument<MyDev::OP1>
{
    int N;
    uint32_t data;
};

template<>
template<>
int KDevice<MyDev>::parse_arg<MyDev::OP1>(const Command& cmd, KDevice<MyDev>::Argument<MyDev::OP1>& args) {

}

template<>
template<>
int KDevice<MyDev>::execute<MyDev::OP1>(const KDevice<MyDev>::Argument<MyDev::OP1>& args) {

}

#endif

} // namespace kserver

#endif // __KDEVICE_HPP__

