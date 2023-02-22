#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_
#ifdef _WIN32
	#define FD_SETSIZE 10000
	#define _WINSOCK_DEPRECATED_NO_WARNINGS		//解决inet_ntoa不能使用的问题
	#define _CRT_SECURE_NO_WARNINGS				//解决scanf strcpy报错的问题
	#define WIN32_LEAN_AND_MEAN					//防止Windows和WinSock头文件冲突
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
#include<vector>
#include"MessageHeader.hpp"
#include"CELLTimestamp.hpp"
//缓冲区最小单元大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif // !RECV_BUFF_SIZE
class ClientSocket {
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET) {
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
		_lastPos = 0;
	}
	SOCKET sockfd() {
		return _sockfd;
	}
	char* msgBuf() {
		return _szMsgBuf;
	}
	int getLastPos() {
		return _lastPos;
	}
	void setLastPos(int pos) {
		_lastPos = pos;
	}
private:
	SOCKET _sockfd;//socket fd_set   file desc set
	//第二缓冲区  消息缓冲区
	char _szMsgBuf[RECV_BUFF_SIZE * 10];
	//消息缓冲区数据尾部数据
	int _lastPos;
};

//new 堆内存
class EasyTcpServer
{
	SOCKET _sock;
	std::vector<ClientSocket*> _clients;
	CELLTimestamp _tTime;
	int _recvCount;
public:
	EasyTcpServer() {
		_sock = INVALID_SOCKET;
		_recvCount = 0;
	}
	virtual~EasyTcpServer() {
		delete[] _szRecv;
		Close();
	}
	//初始化Socket
	SOCKET InitSocket() {
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
			printf("错误，建立socket=<%d>失败....\n", (int)_sock);
		}
		else {
			printf("建立socket=<%d>成功....\n", (int)_sock);
		}
		return _sock;
	}

	//绑定IP和端口号
	int Bind(const char* ip, unsigned short port) {
		if (INVALID_SOCKET == _sock) {
			InitSocket();
		}
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;    //类型
		_sin.sin_port = htons(port);//host to net unsigned short

#ifdef _WIN32
		if (ip)
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);
		else
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;//inet_addr("127.0.0.1");    //ip地址
#else
		if (ip)
			_sin.sin_addr.s_addr = inet_addr(ip);
		else
			_sin.sin_addr.s_addr = INADDR_ANY;//inet_addr("127.0.0.1");    //ip地址
#endif
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
		if (SOCKET_ERROR == ret) {
			printf("错误,绑定网络端口<%d>失败...\n", port);
			//端口被占用
		}
		else {
			printf("绑定网络端口<%d>成功...\n", port);
		}
		return ret;
	}
	//监听端口号
	int Listen(int n) {
		//backlog:需要等待多少人进行链接
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret) {
			printf("socket=<%d>错误,监听网络端口失败...\n", (int)_sock);
		}
		else {
			printf("socket=<%d>监听网络端口成功...\n", (int)_sock);
		}
		return ret;
	}
	//接收客户端链接
	SOCKET Accept() {
		// 4 accept 等待接受客户端连接
		sockaddr_in clientAddr = {};    //用于接收返回的数据
		int nAddrLen = sizeof(sockaddr_in);//返回数据的长度
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif
		if (cSock == INVALID_SOCKET) {//无效的socket
			printf("socket=<%d>错误,接收到无效客户端SOCKET...\n", (int)_sock);
		}
		else {
			/*
			NewUserJoin userJoin;
			userJoin.Sock = cSock;
			SendDataToAll(&userJoin);
			*/
			_clients.push_back(new ClientSocket(cSock));
			printf("socket=<%d>新客户端<%d>加入：socket = %d, IP = %s \n", (int)_sock, (int)_clients.size(), (int)cSock, inet_ntoa(clientAddr.sin_addr));
		}
		return cSock;
	}
	//关闭Socket
	void Close() {
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			// 8关闭套节字closesocket
			closesocket(_sock);
			// ------------------------
			//清除Windows socket环境
			WSACleanup();
#else
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			// 8关闭套节字closesocket
			close(_sock);
			// ------------------------
#endif
			_clients.clear();
		}
	}
	//处理网络消息
	bool OnRun() {
		if (isRun()) {
			//伯克利 socket集合
			fd_set fdRead;//描述符(socket)集合
			fd_set fdWrite;
			fd_set fdExp;
			//清理集合
			FD_ZERO(&fdRead);
			FD_ZERO(&fdWrite);
			FD_ZERO(&fdExp);
			//将描述符(socket)加入集合
			FD_SET(_sock, &fdRead);
			FD_SET(_sock, &fdWrite);
			FD_SET(_sock, &fdExp);
			SOCKET maxSock = _sock;
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				FD_SET(_clients[n]->sockfd(), &fdRead);
				if (maxSock < _clients[n]->sockfd()) {
					maxSock = _clients[n]->sockfd();
				}
			}
			///nfds 是一个整数值 是fd_set集合中所有描述符(socket)的范围,而不是数量
			///既是所有文件描述)最大值(maxSock)+1  在Windows中这个参数可以为0
			timeval t = { 1,0 };//非阻塞
			int ret = (int)select(maxSock + 1, &fdRead, &fdWrite, &fdExp, &t);//if(timeout==NULL) 没有数据时则会造成阻塞
			if (ret < 0) {
				printf("select任务结束.\n");
				Close();
				return false;
			}
			//判断描述符(socket)是否在集合中
			if (FD_ISSET(_sock, &fdRead)) {
				//清零，接入客户端无需进入处理请求
				FD_CLR(_sock, &fdRead);
				Accept();
				return true;
			}
			//6.处理请求，当接到命令时，进入该循环处理
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				if (FD_ISSET(_clients[n]->sockfd(), &fdRead)) {
					if (RecvData(_clients[n]) == -1) {
						auto iter = _clients.begin() + n;
						if (iter != _clients.end()) {
							delete _clients[n];
							_clients.erase(iter);
						}
					}
				}
			}
			return true;
		}
		return false;

	}
	//是否工作中
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}
	//接收数据 处理粘包 拆分包
	//缓冲区
	char* _szRecv = new char[RECV_BUFF_SIZE];
	int RecvData(ClientSocket* pClient) {
		
		//5接受客户端数据
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0) {
			printf("客户端<Socket=%d>已退出,任务结束\n", (int)pClient->sockfd());
			return -1;
		}
		//将收取的数据拷贝到消息缓冲区
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen);
		//消息缓冲区的数据尾部位置后移
		pClient->setLastPos(pClient->getLastPos() + nLen);
		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		while (pClient->getLastPos() >= sizeof(DataHeader)) {
			//这时就可以知道当前消息体的长度
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//判断消息缓冲区的数据长度大于消息长度
			if (pClient->getLastPos() >= header->dataLength) {
				//消息缓冲区剩余未处理数据的长度
				int nSize = pClient->getLastPos() - header->dataLength;
				//处理网络消息
				OnNetMsg(pClient->sockfd(),header);
				//将消息缓冲区剩余未处理的数据前移
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength,nSize);
				//将消息缓冲区的数据尾部位置前移
				pClient->setLastPos(nSize);
			}
			else {
				//消息缓冲区剩余数据不够一条完整信息
				break;
			}
		}
		return 0;
	}
	//响应网络消息
	virtual void OnNetMsg(SOCKET cSock, DataHeader * header) {
		_recvCount++;
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0) {
			printf("time<%lf>,socket<%d>,clients<%d>,_recvCount<%d>\n", t1, (int)_sock, (int)_clients.size(), _recvCount);
			_tTime.update();
			_recvCount = 0;
		}
		switch (header->cmd)
		{
		case CMD_LOGIN: {

			Login* login = (Login*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGIN,数据长度：%d,userName = %s  PassaWord=%s\n", (int)cSock, login->dataLength, login->userName, login->PassWord);
			//忽略判断用户密码是否正确的过程
			//LoginResult ret;
			//SendData(cSock, &ret);
		}
						break;
		case CMD_LOGOUT: {
			Logout* logout = (Logout*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGOUT,数据长度：%d,userName = %s \n", (int)cSock, logout->dataLength, logout->userName);
			//忽略判断用户密码是否正确的过程
			//LogoutResult ret;
			//SendData(cSock, &ret);

		}
						break;
		default: {
			printf("Socket=<%d>收到未定义消息,数据长度：%d\n", (int)cSock, header->dataLength);
			/*DataHeader ret;
			SendData(cSock, &ret); */
		}
				break;
		}
	}
	//发送指定Socket数据
	int SendData(SOCKET cSock, DataHeader * header) {
		if (isRun() && header) {
			return send(cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
	//发送指定Socket数据
	void SendDataToAll(DataHeader * header) {
		for (int n = (int)_clients.size() - 1; n >= 0; n--) {
			SendData(_clients[n]->sockfd(), header);
		}
	}
};

#endif // !_EasyTcpServer_hpp_
