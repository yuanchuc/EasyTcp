#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_
#ifdef _WIN32
	#define FD_SETSIZE 2506
	#define _WINSOCK_DEPRECATED_NO_WARNINGS		//���inet_ntoa����ʹ�õ�����
	#define _CRT_SECURE_NO_WARNINGS				//���scanf strcpy���������
	#define WIN32_LEAN_AND_MEAN					//��ֹWindows��WinSockͷ�ļ���ͻ
	#include<Windows.h>
	#include<WinSock2.h>
	#pragma comment(lib,"ws2_32.lib")//����ϵͳ������
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
//��������С��Ԫ��С
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif // !RECV_BUFF_SIZE
#define _CellServer_THREAD_COUNT 4

//�ͻ�����������
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
	//�ڶ�������  ��Ϣ������
	char _szMsgBuf[RECV_BUFF_SIZE * 10];
	//��Ϣ����������β������
	int _lastPos;
};

//�����¼��ӿ�
class INetEvent
{
public:
	//�ͻ����뿪�¼�
	virtual void OnLeave(ClientSocket* pClient) = 0;
	//�ͻ�����Ϣ�¼�
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
	//�ر�Socket
	void Close() {
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			// 8�ر��׽���closesocket
			closesocket(_sock);
#else
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			// 8�ر��׽���closesocket
			close(_sock);
			// ------------------------
#endif
			_clients.clear();
		}
	}
	//�Ƿ�����
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}
	//����������Ϣ
	bool OnRun() {
		
		while(isRun()) {
			if (_clientsBuff.size() > 0) {
				//�ӻ��������ȡ���ͻ�����
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff) {
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}
			//���û����Ҫ����Ŀͻ���,������
			if (_clients.size()==0) {
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
			//������ socket����
			fd_set fdRead;//������(socket)����
			//������
			FD_ZERO(&fdRead);
			//��������(socket)���뼯��

			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				FD_SET(_clients[n]->sockfd(), &fdRead);
				if (maxSock < _clients[n]->sockfd()) {
					maxSock = _clients[n]->sockfd();
				}
			}
			///nfds ��һ������ֵ ��fd_set����������������(socket)�ķ�Χ,����������
			///���������ļ�����)���ֵ(maxSock)+1  ��Windows�������������Ϊ0
			//timeval t = { 1,0 };//������
			int ret = (int)select(maxSock + 1, &fdRead, nullptr, nullptr, nullptr);//if(timeout==NULL) û������ʱ����������
			if (ret < 0) {
				printf("select�������.\n");
				Close();
				return false;
			}
			//6.�������󣬵��ӵ�����ʱ�������ѭ������
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
	//������
	char* _szRecv = new char[RECV_BUFF_SIZE];
	int RecvData(ClientSocket* pClient) {

		//5���ܿͻ�������
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0) {
			//printf("�ͻ���<Socket=%d>���˳�,�������\n", (int)pClient->sockfd());
			return -1;
		}
		//����ȡ�����ݿ�������Ϣ������
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen);
		//��Ϣ������������β��λ�ú���
		pClient->setLastPos(pClient->getLastPos() + nLen);
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����
		while (pClient->getLastPos() >= sizeof(DataHeader)) {
			//��ʱ�Ϳ���֪����ǰ��Ϣ��ĳ���
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (pClient->getLastPos() >= header->dataLength) {
				//��Ϣ������ʣ��δ�������ݵĳ���
				int nSize = pClient->getLastPos() - header->dataLength;
				//����������Ϣ
				OnNetMsg(pClient->sockfd(), header);
				//����Ϣ������ʣ��δ���������ǰ��
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				//����Ϣ������������β��λ��ǰ��
				pClient->setLastPos(nSize);
			}
			else {
				//��Ϣ������ʣ�����ݲ���һ��������Ϣ
				break;
			}
		}
		return 0;
	}
	//��Ӧ������Ϣ
	virtual void OnNetMsg(SOCKET cSock, DataHeader* header) {
		_pNetEvent->OnNetMsg(cSock, header);
		switch (header->cmd)
		{
		case CMD_LOGIN: {

			Login* login = (Login*)header;
			//printf("�յ��ͻ���<Socket=%d>����CMD_LOGIN,���ݳ��ȣ�%d,userName = %s  PassaWord=%s\n", (int)cSock, login->dataLength, login->userName, login->PassWord);
			//�����ж��û������Ƿ���ȷ�Ĺ���
			//LoginResult ret;
			//SendData(cSock, &ret);
		}
					  break;
		case CMD_LOGOUT: {
			Logout* logout = (Logout*)header;
			//printf("�յ��ͻ���<Socket=%d>����CMD_LOGOUT,���ݳ��ȣ�%d,userName = %s \n", (int)cSock, logout->dataLength, logout->userName);
			//�����ж��û������Ƿ���ȷ�Ĺ���
			//LogoutResult ret;
			//SendData(cSock, &ret);

		}
					   break;
		default: {
			printf("Socket=<%d>�յ�δ������Ϣ,���ݳ��ȣ�%d\n", (int)cSock, header->dataLength);
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
	//����ָ��Socket����
	int SendData(SOCKET cSock, DataHeader* header) {
		if (isRun() && header) {
			return send(cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
	void Start() {
		// �� CellServer::OnRun �����ݳ�Ա���Ǻ��������ڱ���ʱʧ��
		static_assert(std::is_member_function_pointer<decltype(&CellServer::OnRun)>::value,
			"CellServer::OnRun is not a member function.");
		_thread =  std::thread(&CellServer::OnRun, this);
	}
	size_t getClientCount() {
		return _clients.size()+_clientsBuff.size();
	}
private:
	SOCKET _sock;
	//��ʽ�ͻ�����
	std::vector<ClientSocket*> _clients;
	//�ͻ��������
	std::vector<ClientSocket*> _clientsBuff;
	//������е���
	std::mutex _mutex;
	std::thread _thread;
	//�����¼�����
	INetEvent* _pNetEvent;
};

class EasyTcpServer:public INetEvent
{
	SOCKET _sock;
	//��Ϣ�������,�ڲ��ᴴ���߳�
	std::vector<CellServer*> _cellServers;
	//ÿ����Ϣ��ʱ
	CELLTimestamp _tTime;
	//�յ���Ϣ����
	std::atomic<int> _recvCount;
	//�ͻ��˼���
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
	//��ʼ��Socket
	SOCKET InitSocket() {
#ifdef _WIN32
		//����Windows socket 2.x����
		WORD ver = MAKEWORD(2, 2);//�����汾�ţ�MAKEWORD(2,x)��
		WSADATA dat;    //LP��ͷ������LPWSADATA,����ҪдLP
		WSAStartup(ver, &dat);    //����socket���绷��
#endif
		 // 1����һ��socket
		if (INVALID_SOCKET != _sock) {
			printf("<socket = %d>�رվ�����\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock) {
			printf("���󣬽���socket=<%d>ʧ��....\n", (int)_sock);
		}
		else {
			printf("����socket=<%d>�ɹ�....\n", (int)_sock);
		}
		return _sock;
	}
	//��IP�Ͷ˿ں�
	int Bind(const char* ip, unsigned short port) {
		if (INVALID_SOCKET == _sock) {
			InitSocket();
		}
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;    //����
		_sin.sin_port = htons(port);//host to net unsigned short

#ifdef _WIN32
		if (ip)
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);
		else
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;//inet_addr("127.0.0.1");    //ip��ַ
#else
		if (ip)
			_sin.sin_addr.s_addr = inet_addr(ip);
		else
			_sin.sin_addr.s_addr = INADDR_ANY;//inet_addr("127.0.0.1");    //ip��ַ
#endif
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
		if (SOCKET_ERROR == ret) {
			printf("����,������˿�<%d>ʧ��...\n", port);
			//�˿ڱ�ռ��
		}
		else {
			printf("������˿�<%d>�ɹ�...\n", port);
		}
		return ret;
	}
	//�����˿ں�
	int Listen(int n) {
		//backlog:��Ҫ�ȴ������˽�������
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret) {
			printf("socket=<%d>����,��������˿�ʧ��...\n", (int)_sock);
		}
		else {
			printf("socket=<%d>��������˿ڳɹ�...\n", (int)_sock);
		}
		return ret;
	}
	//���տͻ�������
	SOCKET Accept() {
		// 4 accept �ȴ����ܿͻ�������
		sockaddr_in clientAddr = {};    //���ڽ��շ��ص�����
		int nAddrLen = sizeof(sockaddr_in);//�������ݵĳ���
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif
		if (cSock == INVALID_SOCKET) {//��Ч��socket
			printf("socket=<%d>����,���յ���Ч�ͻ���SOCKET...\n", (int)_sock);
		}
		else {
			//���¿ͻ��˷�����ͻ��������ٵ�cellServer
			addClientToCellServer(new ClientSocket(cSock));
			//��ȡIP��ַ  inet_ntoa(clientAddr.sin_addr)
		}
		return cSock;
	}
	//���ҿͻ��������ٵ�CellServer��Ϣ�������
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
			//ע�������¼����ն���
			ser->setEventObj(this);
			//���������߳�
			ser->Start();
		}
	}
	//�ر�Socket
	void Close() {
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			// 8�ر��׽���closesocket
			closesocket(_sock);
			// ------------------------
			//���Windows socket����
			WSACleanup();
#else
			// 8�ر��׽���closesocket
			close(_sock);
			// ------------------------
#endif
		}
	}
	//����������Ϣ
	bool OnRun() {
		if (isRun()) {
			time4msg();
			//������ socket����
			fd_set fdRead;//������(socket)����
			//������
			FD_ZERO(&fdRead);
			//��������(socket)���뼯��
			FD_SET(_sock, &fdRead);
			///nfds ��һ������ֵ ��fd_set����������������(socket)�ķ�Χ,����������
			///���������ļ�����)���ֵ(maxSock)+1  ��Windows�������������Ϊ0
			timeval t = { 1,0 };//������
			int ret = (int)select(_sock + 1, &fdRead, 0, 0, &t);//if(timeout==NULL) û������ʱ����������
			if (ret < 0) {
				printf("Accept select�������.\n");
				Close();
				return false;
			}
			//�ж�������(socket)�Ƿ��ڼ�����
			if (FD_ISSET(_sock, &fdRead)) {
				//���㣬����ͻ���������봦������
				FD_CLR(_sock, &fdRead);
				Accept();
				return true;
			}
			return true;
		}
		return false;

	}
	//�Ƿ�����
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}
	//���㲢���ÿ���յ�����������
	virtual void time4msg() {
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0){
			printf("thread<%d>time<%lf>,socket<%d>,clients<%d>,_recvCount<%d>\n",(int) _cellServers.size() , t1, (int)_sock, (int)_clientCount, (int)(_recvCount/t1));
			_recvCount = 0;
			_tTime.update();
		}
	}
	//����ָ��Socket����
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
