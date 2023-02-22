#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_
#ifdef _WIN32
	#define FD_SETSIZE 4096
	#define WIN32_LEAN_AND_MEAN					//避免Windows库与WinSock2库冲突
	#define _WINSOCK_DEPRECATED_NO_WARNINGS		//解决inet_ntoa不能使用的问题
	#define _CRT_SECURE_NO_WARNINGS
	//socket通讯需要包含的头文件
	#include<Windows.h>
	#include<WinSock2.h>
	#pragma comment(lib,"ws2_32.lib")//其他系统不适配
#else
	#include<unistd.h> //uni std
	#include<arpa/inet.h>
	#include<string.h>
	#define SOCKET int
	#define INVALID_SOCKET (SOCKET)(~0)
	#define SOCKET_ERROR          (~1)
#endif
#include<stdio.h>
#include "MessageHeader.hpp"
class EasyTcpClient
{
	SOCKET _sock;
public:
	//初始化网络环境
	EasyTcpClient() {
		_sock = INVALID_SOCKET;
	}
	//虚析构函数
	virtual ~EasyTcpClient() {
		delete[] _szMsgBuf;
		delete[] _szRecv;
		Close();
	}
	//初始化socket
	void InitSocket() {
#ifdef _WIN32
		//启动Windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);//创建版本号（MAKEWORD(2,x)）
		WSADATA dat;    //LP开头的类型LPWSADATA,不需要写LP
		WSAStartup(ver, &dat);    //启动socket网络环境
#endif
		 // 1建立一个socket
		if (INVALID_SOCKET != _sock) {
			printf("<socket = %d>关闭旧链接\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock) {
			printf("错误，建立Socket失败....\n");
		}
		else {
			printf("建立Socket=<%d>成功....\n", (int)_sock);
		}
	}
	//连接服务器
	int Connect(const char * ip,unsigned short port) {
		if (INVALID_SOCKET == _sock) {
			InitSocket();
		}
		// 2连接服务器connect
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);
#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret) {
			printf("Socket=<%d>错误，连接服务器<%s:%d>失败....\n", (int)_sock,ip,port);
			Close();
			return -1;
		}
		else {
			printf("Socket=<%d>连接服务器<%s:%d>成功....\n", (int)_sock, ip, port);
		}
		return ret;
	}
	//关闭socket
	void Close() {
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			closesocket(_sock);
			WSACleanup();
#else
			close(_sock);
#endif
		}
		_sock = INVALID_SOCKET;
	}
	//处理网络信息
	bool OnRun() {
		if (isRun()) {
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(_sock, &fdReads);
			timeval t = { 0,0 };
			int ret = (int)select(_sock + 1, &fdReads, NULL, NULL, &t);
			if (ret < 0) {
				printf("<socket = %d>select任务结束\n",(int) _sock);
				Close();
				return false;
			}
			if (FD_ISSET(_sock, &fdReads)) {
				FD_CLR(_sock, &fdReads);
				if (-1 == RecvData(_sock)) {
					printf("<socket = %d>select任务结束2\n", (int)_sock);
					Close();
					return false;
				}
			}
			return true;
		}else{
			return false;
		}
	}
	//是否工作中
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}

	//缓冲区最小单元大小
#ifndef RECV_BUFF_SIZE
	#define RECV_BUFF_SIZE 10240
#endif // !RECV_BUFF_SIZE
	//接收缓冲区
	char* _szRecv = new char[RECV_BUFF_SIZE * 10];
	//第二缓冲区  消息缓冲区
	char* _szMsgBuf = new char[RECV_BUFF_SIZE * 10];
	//消息缓冲区数据尾部数据
	int _lastPos = 0;
	//接收数据 处理粘包 拆分包
	int RecvData(SOCKET cSock) {
		
		//5接受数据
		int nLen = (int)recv(cSock, _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0) {
			printf("Socket=<%d>与服务器断开连接,任务结束。\n", (int)cSock);
			return -1;
		}
		//将收取的数据拷贝到消息缓冲区
		memcpy(_szMsgBuf + _lastPos, _szRecv, nLen);
		//消息缓冲区的数据尾部位置后移
		_lastPos += nLen;
		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		while(_lastPos >= sizeof(DataHeader)) {
			//这时就可以知道当前消息体的长度
			DataHeader* header = (DataHeader*)_szMsgBuf;
			//判断消息缓冲区的数据长度大于消息长度
			if (_lastPos >= header->dataLength) {
				//消息缓冲区剩余未处理数据的长度
				int nSize = _lastPos - header->dataLength;
				//处理网络消息
				OnNetMsg(header);
				//将消息缓冲区剩余未处理的数据前移
				memcpy(_szMsgBuf, _szMsgBuf+header->dataLength, nSize);
				//将消息缓冲区的数据尾部位置前移
				_lastPos = nSize;
			}
			else {
				//消息缓冲区剩余数据不够一条完整信息
				break;
			}
		}
		
		//recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);//由于 继承的原因，需要偏移
		//OnNetMsg(header);
		return 0;
	}
	//响应网络消息
	virtual void OnNetMsg(DataHeader* header) {
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT: {
			LogoutResult* logout = (LogoutResult*)header;
			//printf("Socket=<%d>收到服务器返回消息：CMD_LOGIN_RESULT,数据长度：%d\n",_sock, logout->dataLength);
		}
							 break;
		case CMD_LOGOUT_RESULT: {
			LoginResult* login = (LoginResult*)header;
			//printf("Socket=<%d>收到服务器返回消息：CMD_LOGOUT_RESULT,数据长度：%d\n", _sock, login->dataLength);
		}
							  break;
		case CMD_NEW_USER_JOIN: {
			NewUserJoin* userJoin = (NewUserJoin*)header;
			//printf("Socket=<%d>收到服务器返回消息：CMD_NEW_USER_JOIN,数据长度：%d\n", _sock, userJoin->dataLength);
		}
							  break;
		case CMD_ERROR: {
			printf("Socket=<%d>收到服务器返回消息：CMD_ERROR,数据长度：%d\n", (int)_sock, header->dataLength);
			
		}break;
		default: {
			printf("Socket=<%d>收到未定义消息,数据长度：%d\n", (int)_sock, header->dataLength);

		}break;
		}
	}
	//发送数据
	int SendData(DataHeader* header) {
		if (isRun() && header) {
			return send(_sock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
private:

};



#endif // !Ea
