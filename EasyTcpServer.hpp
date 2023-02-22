#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_
#ifdef _WIN32
	#define FD_SETSIZE 2506
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
#include<thread>
#include<mutex>
#include<atomic>
#include"MessageHeader.hpp"
#include"CELLTimestamp.hpp"
//缓冲区最小单元大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif // !RECV_BUFF_SIZE
#define _CellServer_THREAD_COUNT 4

//客户端数据类型
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

//网络事件接口
class INetEvent
{
public:
	//客户端离开事件
	virtual void OnLeave(ClientSocket* pClient) = 0;
	//客户端消息事件
	virtual void OnNetMsg(SOCKET cSock, DataHeader* header) = 0;
private:

};

class CellServer
{
	
public:
	CellServer(SOCKET sock = INVALID_SOCKET) {
		_sock = sock;
		_pNetEvent = nullptr;
	} 

	~CellServer() {
		Close();
		delete[] _szRecv;
		_sock = INVALID_SOCKET;
	}
	void setEventObj(INetEvent * event) {
		_pNetEvent = event;
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
	//是否工作中
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}
	//处理网络消息
	bool OnRun() {
		
		while(isRun()) {
			if (_clientsBuff.size() > 0) {
				//从缓冲队列中取出客户数据
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff) {
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}
			//如果没有需要处理的客户端,就跳过
			if (_clients.size()==0) {
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
			//伯克利 socket集合
			fd_set fdRead;//描述符(socket)集合
			//清理集合
			FD_ZERO(&fdRead);
			//将描述符(socket)加入集合

			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				FD_SET(_clients[n]->sockfd(), &fdRead);
				if (maxSock < _clients[n]->sockfd()) {
					maxSock = _clients[n]->sockfd();
				}
			}
			///nfds 是一个整数值 是fd_set集合中所有描述符(socket)的范围,而不是数量
			///既是所有文件描述)最大值(maxSock)+1  在Windows中这个参数可以为0
			//timeval t = { 1,0 };//非阻塞
			int ret = (int)select(maxSock + 1, &fdRead, nullptr, nullptr, nullptr);//if(timeout==NULL) 没有数据时则会造成阻塞
			if (ret < 0) {
				printf("select任务结束.\n");
				Close();
				return false;
			}
			//6.处理请求，当接到命令时，进入该循环处理
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				if (FD_ISSET(_clients[n]->sockfd(), &fdRead)) {
					if (RecvData(_clients[n]) == -1) {
						auto iter = _clients.begin() + n;
						if (iter != _clients.end()) {
							_pNetEvent->OnLeave(_clients[n]);
							delete _clients[n];
							_clients.erase(iter);
						}
					}
				}
			}
		}
	}
	//缓冲区
	char* _szRecv = new char[RECV_BUFF_SIZE];
	int RecvData(ClientSocket* pClient) {

		//5接受客户端数据
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0) {
			//printf("客户端<Socket=%d>已退出,任务结束\n", (int)pClient->sockfd());
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
				OnNetMsg(pClient->sockfd(), header);
				//将消息缓冲区剩余未处理的数据前移
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
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
	virtual void OnNetMsg(SOCKET cSock, DataHeader* header) {
		_pNetEvent->OnNetMsg(cSock, header);
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
	void addClient(ClientSocket* pClient) {
		std::lock_guard<std::mutex> lock(_mutex);
		//_mutex.lock();
		_clientsBuff.push_back(pClient);
		//_mutex.lock();
	}
	//发送指定Socket数据
	int SendData(SOCKET cSock, DataHeader* header) {
		if (isRun() && header) {
			return send(cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
	void Start() {
		// 若 CellServer::OnRun 是数据成员而非函数，则在编译时失败
		static_assert(std::is_member_function_pointer<decltype(&CellServer::OnRun)>::value,
			"CellServer::OnRun is not a member function.");
		_thread =  std::thread(&CellServer::OnRun, this);
	}
	size_t getClientCount() {
		return _clients.size()+_clientsBuff.size();
	}
private:
	SOCKET _sock;
	//正式客户队列
	std::vector<ClientSocket*> _clients;
	//客户缓冲队列
	std::vector<ClientSocket*> _clientsBuff;
	//缓冲队列的锁
	std::mutex _mutex;
	std::thread _thread;
	//网络事件对象
	INetEvent* _pNetEvent;
};

class EasyTcpServer:public INetEvent
{
	SOCKET _sock;
	//消息处理对象,内部会创建线程
	std::vector<CellServer*> _cellServers;
	//每秒消息计时
	CELLTimestamp _tTime;
	//收到消息计数
	std::atomic<int> _recvCount;
	//客户端计数
	std::atomic<int> _clientCount;
public:
	EasyTcpServer() {
		_sock = INVALID_SOCKET;
		_recvCount = 0;
		_clientCount = 0;
	}
	virtual~EasyTcpServer() {
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
			//将新客户端分配给客户数量最少的cellServer
			addClientToCellServer(new ClientSocket(cSock));
			//获取IP地址  inet_ntoa(clientAddr.sin_addr)
		}
		return cSock;
	}
	//查找客户数量最少的CellServer消息处理对象
	void addClientToCellServer(ClientSocket* pClient) {
		auto pMinServer = _cellServers[0];
		for (auto pCellServer : _cellServers) {
			if (pMinServer->getClientCount() > pCellServer->getClientCount()) {
				pMinServer = pCellServer;
			}
		}
		pMinServer->addClient(pClient);
		_clientCount++;
	}
	void Start() {
		for (int i = 0; i < _CellServer_THREAD_COUNT; i++)
		{
			auto ser = new CellServer(_sock);
			_cellServers.push_back(ser);
			//注册网络事件接收对象
			ser->setEventObj(this);
			//启动服务线程
			ser->Start();
		}
	}
	//关闭Socket
	void Close() {
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			// 8关闭套节字closesocket
			closesocket(_sock);
			// ------------------------
			//清除Windows socket环境
			WSACleanup();
#else
			// 8关闭套节字closesocket
			close(_sock);
			// ------------------------
#endif
		}
	}
	//处理网络消息
	bool OnRun() {
		if (isRun()) {
			time4msg();
			//伯克利 socket集合
			fd_set fdRead;//描述符(socket)集合
			//清理集合
			FD_ZERO(&fdRead);
			//将描述符(socket)加入集合
			FD_SET(_sock, &fdRead);
			///nfds 是一个整数值 是fd_set集合中所有描述符(socket)的范围,而不是数量
			///既是所有文件描述)最大值(maxSock)+1  在Windows中这个参数可以为0
			timeval t = { 1,0 };//非阻塞
			int ret = (int)select(_sock + 1, &fdRead, 0, 0, &t);//if(timeout==NULL) 没有数据时则会造成阻塞
			if (ret < 0) {
				printf("Accept select任务结束.\n");
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
			return true;
		}
		return false;

	}
	//是否工作中
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}
	//计算并输出每秒收到的网络请求
	virtual void time4msg() {
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0){
			printf("thread<%d>time<%lf>,socket<%d>,clients<%d>,_recvCount<%d>\n",(int) _cellServers.size() , t1, (int)_sock, (int)_clientCount, (int)(_recvCount/t1));
			_recvCount = 0;
			_tTime.update();
		}
	}
	//发送指定Socket数据
	int SendData(SOCKET cSock, DataHeader* header) {
		if (isRun() && header) {
			return send(cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
	virtual void OnLeave(ClientSocket* pClient) {
		_clientCount--;
	}
	virtual void OnNetMsg(SOCKET cSock, DataHeader* header) {
		_recvCount++;
	}
};

#endif // !_EasyTcpServer_hpp_
