/***********************************************************************************
 * Copyright (c) 2012, Sepehr Taghdisian
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 ***********************************************************************************/

#ifndef __NETSOCKET_H__
#define __NETSOCKET_H__

#include "types.h"
#include "core-api.h"

/**
 * @defgroup socket Sockets
 */

#if defined(_WIN_)
#include <WinSock2.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

#if defined(_WIN_)
#define SOCK_NULL      INVALID_SOCKET
#define SOCK_ERROR     SOCKET_ERROR
#else
#define SOCK_NULL      -1
#define SOCK_ERROR     -1
#endif

/**
 * initialize sockets API
 * used for WinSock api, and doesn't do anything under unix
 * @ingroup socket
 */
CORE_API result_t sock_init();
/**
 * release sockets API
 * @ingroup socket
 */
CORE_API void sock_release();

/**
 * get current host name
 * @ingroup socket
 */
CORE_API const char* sock_gethostname();
/**
 * resolves ip address of network name (dns resolve)
 * @ingroup socket
 */
CORE_API const char* sock_resolveip(const char* name);

/**
 * creates udp socket, udp sockets are connection-less but not very stable/reliable
 * @ingroup socket
 */
CORE_API socket_t sock_udpcreate();
/**
 * destroy udp socket
 * @ingroup socket
 */
CORE_API void sock_udpdestroy(socket_t sock);
/**
 * binds port to udp socket in order to recv data from that port
 * receiver should always bind a port number to the socket before receiving data
 * @ingroup socket
 */
CORE_API result_t sock_udpbind(socket_t sock, int port);
/**
 * receives data from udp socket
 * @param out_sender_ipaddr ip address of the sender returned
 * @return number of bytes actually received, <=0 if error occured
 * @ingroup socket
 */
CORE_API int sock_udprecv(socket_t sock, void* buffer, int size, char* out_sender_ipaddr);
/**
 * sends data to udp socket
 * @param ipaddr ip address of the target receiver returned
 * @param port port address that target is listening
 * @param buffer buffer to be sent
 * @return number of bytes actually sent, <=0 if error occured
 * @ingroup socket
 */
CORE_API int sock_udpsend(socket_t sock, const char* ipaddr, int port,
                            const void* buffer, int size);

/**
 * create tcp socket, tcp sockets need connection (accept/connect) but are stable and reliable
 * @ingroup socket
 */
CORE_API socket_t sock_tcpcreate();
/**
 * destroy tcp socket
 * @ingroup socket
 */
CORE_API void sock_tcpdestroy(socket_t sock);
/**
 * listens tcp socket as a server and waits (blocks) the program until peer is connected
 * @ingroup socket
 */
CORE_API result_t sock_tcplisten(socket_t sock, int port);
/**
 * accept peer connection
 * @return newly connected/created socket, user should send/recv data with newly created socket
 * @ingroup socket
 */
CORE_API socket_t sock_tcpaccept(socket_t sock, char* out_peer_ipaddr);
/**
 * connect to server (listening) socket, blocks the program until it connects to peer
 * @param ipaddr ip address of peer to connect
 * @param port port number of the connection
 * @ingroup socket
 */
CORE_API result_t sock_tcpconnect(socket_t sock, const char* ipaddr, int port);
/**
 * receives data from tcp peer
 * @param buffer receive buffer, must hold maximum amount of 'size'
 * @param size maximum buffer size (bytes)
 * @return actual bytes that is received. <=0 if error occured
 * @ingroup socket
 */
CORE_API int sock_tcprecv(socket_t sock, void* buffer, int size);
/**
 * sends data to tcp peer
 * @param buffer buffer to be sent
 * @param size size of the send buffer (bytes)
 * @return actual bytes that is sent. <=0 if error occured
 * @ingroup socket
 */
CORE_API int sock_tcpsend(socket_t sock, const void* buffer, int size);
/**
 * blocks the program and checks if socket has input packet for receiving buffer
 * @param timeout timeout in milliseconds
 * @ingroup socket
 */
CORE_API bool_t sock_pollrecv(socket_t sock, uint timeout);
/**
 * blocks the program and checks if we can send data through the socket
 * @param timeout timeout in milliseconds
 * @ingroup socket
 */
CORE_API bool_t sock_pollsend(socket_t sock, uint timeout);

#endif /* __NETSOCKET_H__ */
