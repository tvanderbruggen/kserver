/// @file kdevice.cpp
///
/// @brief Implementation of kdevice.hpp
///
/// @author Thomas Vanderbruggen <thomas@koheron.com>
/// @date 20/12/2014
///
/// (c) Koheron 2014

#include "kdevice.hpp"

#include "../devices/devices_table.hpp"

namespace kserver {

// X Macro: Get is_failed
#define EXPAND_AS_IS_FAILED(num, name, operations ...)          \
        case num:                                               \
            return ((KDevice<name, num>*)this)->is_failed();
      
bool KDeviceAbstract::is_failed(void) 
{
    switch(kind) {
      case NO_DEVICE:
        return 0;
      case KSERVER:
        return ((KDevice<KServer, KSERVER>*)this)->is_failed();
        
      DEVICES_TABLE(EXPAND_AS_IS_FAILED) // X-Macro
            
      case device_num:
      default:
        fprintf(stderr, "is_failed: Unknown device");
        return 0;
    }
}

} // namespace kserver
