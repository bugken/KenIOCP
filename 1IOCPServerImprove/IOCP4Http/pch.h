#ifndef PCH_H
#define PCH_H
//����std::max���ִ���expected an identifier
#define NOMINMAX
#include <WinSock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>     //for inet_ntop
#pragma comment(lib,"ws2_32.lib")

#endif //PCH_H
