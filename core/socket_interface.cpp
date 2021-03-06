/// @file socket_interface.cpp
///
/// @brief Implementation of socket_interface.hpp
///
/// @author Thomas Vanderbruggen <thomas@koheron.com>
/// @date 15/08/2015
///
/// (c) Koheron 2014-2015

#include "socket_interface.hpp"

extern "C" {
  #include <arpa/inet.h>
}

#include "kserver_defs.hpp"

namespace kserver {

#define SEND_SPECIALIZE_IMPL(sock_interf)                           \
    template<>                                                      \
    int sock_interf::Send<std::string>(const std::string& str)      \
    {                                                               \
        return SendCstr(str.c_str());                               \
    }                                                               \
                                                                    \
    template<>                                                      \
    int sock_interf::Send<uint32_t>(const uint32_t& val)            \
    {                                                               \
        return SendArray<uint32_t>(&val, 1);                        \
    }                                                               \
                                                                    \
    template<>                                                      \
    int sock_interf::Send<float>(const float& val)                  \
    {                                                               \
        return SendArray<float>(&val, 1);                           \
    }

// -----------------------------------------------
// TCP
// -----------------------------------------------

#if KSERVER_HAS_TCP

SEND_SPECIALIZE_IMPL(TCPSocketInterface)

int TCPSocketInterface::init(void) {return 0;}
int TCPSocketInterface::exit(void) {return 0;}

int TCPSocketInterface::read_data(char *buff_str, char *remain_str)
{
    bzero(buff_str, 2*KSERVER_READ_STR_LEN);
    bzero(read_str, KSERVER_READ_STR_LEN);
    
    int nb_bytes_rcvd = read(comm_fd, read_str, KSERVER_READ_STR_LEN);

    // Check reception ...
    if(nb_bytes_rcvd < 0) {
        kserver->syslog.print(SysLog::CRITICAL, "Read error\n");
        return -1;
    }

    if(nb_bytes_rcvd == KSERVER_READ_STR_LEN) {
        kserver->syslog.print(SysLog::CRITICAL, "Read buffer overflow\n");
        return -1;
    }

    if(nb_bytes_rcvd == 0) {
        return 1; // Connection closed by client
    }
        
    kserver->syslog.print(SysLog::DEBUG, "[R@%u] [%d bytes]\n", 
                          id, nb_bytes_rcvd);
        
    // Need a copy here for buffers concatenation (needed?) 
    //
    // XXX snprintf maybe not the fastest solution (but very safe)
    // --> Consider using strlen + memcpy
    int ret = snprintf(buff_str, 2*KSERVER_READ_STR_LEN, "%s%s", 
                       remain_str, read_str);
    bzero(read_str, KSERVER_READ_STR_LEN);
	    
    if(ret < 0) {
        kserver->syslog.print(SysLog::CRITICAL, "Format error\n");
    	return -1;
    }

    if(ret >= 2*KSERVER_READ_STR_LEN) {
        kserver->syslog.print(SysLog::CRITICAL, "Buffer buff_str overflow\n");
        return -1;
    }
    
    return 0;
}

int TCPSocketInterface::RcvDataBuffer(uint32_t n_bytes)
{
    uint32_t bytes_read = 0;
	
    while (bytes_read < n_bytes) {
        int result = read(comm_fd, recv_data_buff + bytes_read,
                          n_bytes-bytes_read);
        // Check reception ...
        if(result < -1) {
            kserver->syslog.print(SysLog::ERROR, "Read error\n");
            return -1;
        }
		
        if(result == 0) {
            kserver->syslog.print(SysLog::WARNING, 
                                  "Connection closed by client\n");
            return -1;
        }
		
        bytes_read += result;
        
        if(bytes_read > KSERVER_RECV_DATA_BUFF_LEN) {
            kserver->syslog.print(SysLog::CRITICAL, 
                                  "Receive data buffer overflow\n");
            return -1;
        }
    }
    
    assert(bytes_read == n_bytes);
	
    return bytes_read;
}

const uint32_t* TCPSocketInterface::RcvHandshake(uint32_t buff_size)
{
    // Handshaking
    if(Send<uint32_t>(htonl(buff_size)) < 0) {
        kserver->syslog.print(SysLog::ERROR, "Cannot send buffer size\n");
        return nullptr;
    }
        
    uint32_t n_bytes_received = 
        static_cast<uint32_t>(RcvDataBuffer(sizeof(uint32_t)*buff_size));
	        
    if(n_bytes_received < 0) {
        return nullptr;
    }
        
    kserver->syslog.print(SysLog::DEBUG, "[R@%u] [%u bytes]\n", 
                          id, n_bytes_received);
    
    return reinterpret_cast<const uint32_t*>(recv_data_buff);
}

int TCPSocketInterface::SendCstr(const char *string)
{
    int bytes_send = strlen(string) + 1;
    int err = write(comm_fd, string, bytes_send);
    
    if(err < 0) {
        kserver->syslog.print(SysLog::ERROR, 
                              "SendCstr: Can't write to client\n");
        return -1;
    }
		
    return bytes_send;
}

#endif // KSERVER_HAS_TCP

// -----------------------------------------------
// WebSocket
// -----------------------------------------------

#if KSERVER_HAS_WEBSOCKET

SEND_SPECIALIZE_IMPL(WebSocketInterface)

int WebSocketInterface::init(void)
{    
    websock.set_id(comm_fd);
            
    if(websock.authenticate() < 0) {
        kserver->syslog.print(SysLog::CRITICAL, 
                              "Cannot connect websocket to client\n");	
        return -1;
    }
	
	return 0;
}

int WebSocketInterface::exit(void) {return 0;}

int WebSocketInterface::read_data(char *buff_str, char *remain_str)
{
    bzero(buff_str, 2*KSERVER_READ_STR_LEN);
    
    if(websock.receive() < 0) { 
        if(websock.is_closed())
            return 1; // Connection closed by client
        else
            return -1;
    }
        
    if(websock.get_payload(buff_str, 2*KSERVER_READ_STR_LEN) < 0) {
        return -1;                                  
    }
    
    return 0;
}

const uint32_t* WebSocketInterface::RcvHandshake(uint32_t buff_size)
{
    // Handshaking
    if(Send<uint32_t>(buff_size) < 0) {
        kserver->syslog.print(SysLog::ERROR, "Error sending the buffer size\n");
        return nullptr;
    }
        
    int payload_size = websock.receive();
        
    if(payload_size < 0) { 
        return nullptr;
    }
        
    if(static_cast<uint32_t>(payload_size) != sizeof(uint32_t)*buff_size) {
        kserver->syslog.print(SysLog::ERROR, "Invalid data size received\n");
        return nullptr;
    }
        
    if(websock.get_payload(recv_data_buff, KSERVER_RECV_DATA_BUFF_LEN) < 0) {
        return nullptr;                                  
    }        
    
    return reinterpret_cast<const uint32_t*>(recv_data_buff);
}

int WebSocketInterface::SendCstr(const char *string)
{
    int err = websock.send(std::string(string));
    
    if(err < 0) {
        kserver->syslog.print(SysLog::ERROR, 
                              "SendCstr: Can't write to client\n");
        return -1;
    }
		
    return strlen(string) + 1;
}

#endif // KSERVER_HAS_WEBSOCKET

} // namespace kserver
