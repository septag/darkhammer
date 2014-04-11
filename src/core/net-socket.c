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

#if defined(_WIN_)
#include "win.h"
#endif

#include "net-socket.h"
#include "str.h"
#include "err.h"

#if !defined(_WIN_)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#define closesocket     close
#else
typedef int   socklen_t;
#endif

result_t sock_init()
{
#if defined(_WIN_)
    WORD version = MAKEWORD(2, 0);
    WSADATA wsaData;

    int r = WSAStartup(version, &wsaData);
    /* check for winsock version 2.0 */
    if (r != 0 || LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 0)    {
        return RET_FAIL;
    }
#endif
    return RET_OK;
}

void sock_release()
{
#if defined(_WIN_)
    WSACleanup();
#endif
}

const char* sock_gethostname()
{
    static char name[255];
    gethostname(name, sizeof(name));
    return name;
}

const char* sock_resolveip(const char* name)
{
    char host_name[255];
    if (name == NULL || str_isempty(name) || str_isequal_nocase(name, "localhost")) {
        strcpy(host_name, sock_gethostname());
    }    else    {
        strcpy(host_name, name);
    }

    /* return first binded ip address to the host */
    struct hostent* addr = gethostbyname(host_name);
    if (addr == NULL)   return "0.0.0.0";
    else                return inet_ntoa(*((struct in_addr*)addr->h_addr_list[0]));
}

/* udp socket */
socket_t sock_udpcreate()
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}

void sock_udpdestroy(socket_t sock)
{
    if (sock != SOCK_NULL)   {
        closesocket(sock);
        sock = SOCK_NULL;
    }
}

result_t sock_udpbind(socket_t sock, int port)
{
    return RET_OK;
}

int sock_udprecv(socket_t sock, void* buffer, int size, char* out_sender_ipaddr)
{
    if (sock == SOCK_NULL)      return -1;
    struct sockaddr_in addr;
    memset(&addr, 0x00, sizeof(struct sockaddr_in));
    socklen_t addrlen = sizeof(addr);

    int r = recvfrom(sock, (char*)buffer, size, 0, (struct sockaddr*)&addr, &addrlen);
    if (r > 0)     {
        strcpy(out_sender_ipaddr, inet_ntoa(addr.sin_addr));
    }
    return r;
}

int sock_udpsend(socket_t sock, const char* ipaddr, int port,
                   const void* buffer, int size)
{
    if (sock == SOCK_NULL)      return -1;
    int max_size;
    socklen_t var_size = sizeof(int);

    #if defined(_WIN_)
    if (getsockopt(sock, SOL_SOCKET, SO_MAX_MSG_SIZE, (char*)&max_size, &var_size) == SOCK_ERROR)  {
        return 0;
    }
    #else
    if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &max_size, &var_size) == SOCK_ERROR)    {
        return 0;
    }
    #endif

    struct sockaddr_in addr;
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ipaddr);

    /* divide and send packets */
    int sent_bytes = 0;
    if (size <= max_size)   {
        sent_bytes = sendto(sock, (const char*)buffer, size, 0,
                            (struct sockaddr*)&addr, sizeof(addr));
    }    else    {
        int packets_cnt = size / max_size;
        int remain_size = size % max_size;
        int offset = 0;
        for (int i = 0; i < packets_cnt; i++)     {
            sent_bytes += sendto(sock, (const char*)buffer + offset, max_size, 0,
                                 (struct sockaddr*)&addr, sizeof(addr));
            offset += max_size;
        }
        if (remain_size > 0)    {
            sent_bytes += sendto(sock, (const char*)buffer + offset, remain_size, 0,
                                 (struct sockaddr*)&addr, sizeof(addr));
        }
    }

    return sent_bytes;
}


/* tcp socket */
socket_t sock_tcpcreate()
{
    return socket(AF_INET, SOCK_STREAM, 0);
}

void sock_tcpdestroy(socket_t sock)
{
    if (sock != SOCK_NULL)   {
        closesocket(sock);
        sock = SOCK_NULL;
    }
}

result_t sock_tcplisten(socket_t sock, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCK_ERROR)   {
        return RET_FAIL;
    }

    if (listen(sock, 1) == SOCK_ERROR)  {
        return RET_FAIL;
    }

    return RET_OK;
}

socket_t sock_tcpaccept(socket_t sock, char* out_peer_ipaddr)
{
    ASSERT(sock != SOCK_NULL);
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    socket_t ret_sock = accept(sock, (struct sockaddr*)&addr, &addrlen);
    if (ret_sock != SOCK_NULL)  {
        strcpy(out_peer_ipaddr, inet_ntoa(addr.sin_addr));
    }
    return ret_sock;
}

result_t sock_tcpconnect(socket_t sock, const char* ipaddr, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ipaddr);
    int r = connect(sock, (const struct sockaddr*)&addr, sizeof(addr));
    return (r != SOCK_ERROR) ? RET_OK : RET_FAIL;
}

int sock_tcprecv(socket_t sock, void* buffer, int size)
{
    if (sock == SOCK_NULL)      return -1;
    return recv(sock, (char*)buffer, size, 0);
}

int sock_tcpsend(socket_t sock, const void* buffer, int size)
{
    if (sock == SOCK_NULL)      return -1;
    return send(sock, (const char*)buffer, size, 0);
}


int sock_pollrecv(socket_t sock, uint timeout)
{
    struct timeval tmout = {timeout/1000, (timeout%1000)*1000};
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(sock, &readset);

    if (select((int)(sock+1), &readset, NULL, NULL, &tmout) > 0 && FD_ISSET(sock, &readset))    {
        return TRUE;
    }

    return FALSE;
}

int sock_pollsend(socket_t sock, uint timeout)
{
    struct timeval tmout = {timeout/1000, (timeout%1000)*1000};
    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(sock, &writeset);

    if (select((int)(sock+1), NULL, &writeset, NULL, &tmout) > 0 && FD_ISSET(sock, &writeset))  {
        return TRUE;
    }

    return FALSE;
}
