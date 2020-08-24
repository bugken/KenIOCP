#ifndef __NET_H__
#define __NET_H__
#include <WinSock2.h>

struct Network
{
    static bool init();
    static bool unInit();

    static SOCKET socket();
    static int bind(SOCKET s, const LPSOCKADDR_IN pAddr);
    static int listen(SOCKET s, int backlog = SOMAXCONN);

    static SOCKADDR_IN getsockname(SOCKET s);
    static SOCKADDR_IN getpeername(SOCKET s);
	
    static bool setKeepAlive(SOCKET s, bool on);
    //Ĭ��ǿ�ƹرգ��������ã�
    static bool setLinger(SOCKET s, bool on = true, int timeoutSecs = 0);

    /*
    * When the AcceptEx function returns, the socket sAcceptSocket 
	*	is in the default state for a connected socket.
    * The socket sAcceptSocket does not inherit the properties 
	*	of the socket associated with sListenSocket parameter
    *	until SO_UPDATE_ACCEPT_CONTEXT is set on the socket.
    */
    static bool updateAcceptContext(SOCKET listenSocket, SOCKET acceptSocket);
};

#endif // !__NET_H__


