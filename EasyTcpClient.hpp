#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_
#ifdef _WIN32
	#define FD_SETSIZE 4096
	#define WIN32_LEAN_AND_MEAN					//����Windows����WinSock2���ͻ
	#define _WINSOCK_DEPRECATED_NO_WARNINGS		//���inet_ntoa����ʹ�õ�����
	#define _CRT_SECURE_NO_WARNINGS
	//socketͨѶ��Ҫ������ͷ�ļ�
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
#include "MessageHeader.hpp"
class EasyTcpClient
{
	SOCKET _sock;
public:
	//��ʼ�����绷��
	EasyTcpClient() {
		_sock = INVALID_SOCKET;
	}
	//����������
	virtual ~EasyTcpClient() {
		delete[] _szMsgBuf;
		delete[] _szRecv;
		Close();
	}
	//��ʼ��socket
	void InitSocket() {
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
			printf("���󣬽���Socketʧ��....\n");
		}
		else {
			printf("����Socket=<%d>�ɹ�....\n", (int)_sock);
		}
	}
	//���ӷ�����
	int Connect(const char * ip,unsigned short port) {
		if (INVALID_SOCKET == _sock) {
			InitSocket();
		}
		// 2���ӷ�����connect
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
			printf("Socket=<%d>�������ӷ�����<%s:%d>ʧ��....\n", (int)_sock,ip,port);
			Close();
			return -1;
		}
		else {
			printf("Socket=<%d>���ӷ�����<%s:%d>�ɹ�....\n", (int)_sock, ip, port);
		}
		return ret;
	}
	//�ر�socket
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
	//����������Ϣ
	bool OnRun() {
		if (isRun()) {
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(_sock, &fdReads);
			timeval t = { 0,0 };
			int ret = (int)select(_sock + 1, &fdReads, NULL, NULL, &t);
			if (ret < 0) {
				printf("<socket = %d>select�������\n",(int) _sock);
				Close();
				return false;
			}
			if (FD_ISSET(_sock, &fdReads)) {
				FD_CLR(_sock, &fdReads);
				if (-1 == RecvData(_sock)) {
					printf("<socket = %d>select�������2\n", (int)_sock);
					Close();
					return false;
				}
			}
			return true;
		}else{
			return false;
		}
	}
	//�Ƿ�����
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}

	//��������С��Ԫ��С
#ifndef RECV_BUFF_SIZE
	#define RECV_BUFF_SIZE 10240
#endif // !RECV_BUFF_SIZE
	//���ջ�����
	char* _szRecv = new char[RECV_BUFF_SIZE * 10];
	//�ڶ�������  ��Ϣ������
	char* _szMsgBuf = new char[RECV_BUFF_SIZE * 10];
	//��Ϣ����������β������
	int _lastPos = 0;
	//�������� ����ճ�� ��ְ�
	int RecvData(SOCKET cSock) {
		
		//5��������
		int nLen = (int)recv(cSock, _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0) {
			printf("Socket=<%d>��������Ͽ�����,���������\n", (int)cSock);
			return -1;
		}
		//����ȡ�����ݿ�������Ϣ������
		memcpy(_szMsgBuf + _lastPos, _szRecv, nLen);
		//��Ϣ������������β��λ�ú���
		_lastPos += nLen;
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����
		while(_lastPos >= sizeof(DataHeader)) {
			//��ʱ�Ϳ���֪����ǰ��Ϣ��ĳ���
			DataHeader* header = (DataHeader*)_szMsgBuf;
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (_lastPos >= header->dataLength) {
				//��Ϣ������ʣ��δ�������ݵĳ���
				int nSize = _lastPos - header->dataLength;
				//����������Ϣ
				OnNetMsg(header);
				//����Ϣ������ʣ��δ���������ǰ��
				memcpy(_szMsgBuf, _szMsgBuf+header->dataLength, nSize);
				//����Ϣ������������β��λ��ǰ��
				_lastPos = nSize;
			}
			else {
				//��Ϣ������ʣ�����ݲ���һ��������Ϣ
				break;
			}
		}
		
		//recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);//���� �̳е�ԭ����Ҫƫ��
		//OnNetMsg(header);
		return 0;
	}
	//��Ӧ������Ϣ
	virtual void OnNetMsg(DataHeader* header) {
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT: {
			LogoutResult* logout = (LogoutResult*)header;
			//printf("Socket=<%d>�յ�������������Ϣ��CMD_LOGIN_RESULT,���ݳ��ȣ�%d\n",_sock, logout->dataLength);
		}
							 break;
		case CMD_LOGOUT_RESULT: {
			LoginResult* login = (LoginResult*)header;
			//printf("Socket=<%d>�յ�������������Ϣ��CMD_LOGOUT_RESULT,���ݳ��ȣ�%d\n", _sock, login->dataLength);
		}
							  break;
		case CMD_NEW_USER_JOIN: {
			NewUserJoin* userJoin = (NewUserJoin*)header;
			//printf("Socket=<%d>�յ�������������Ϣ��CMD_NEW_USER_JOIN,���ݳ��ȣ�%d\n", _sock, userJoin->dataLength);
		}
							  break;
		case CMD_ERROR: {
			printf("Socket=<%d>�յ�������������Ϣ��CMD_ERROR,���ݳ��ȣ�%d\n", (int)_sock, header->dataLength);
			
		}break;
		default: {
			printf("Socket=<%d>�յ�δ������Ϣ,���ݳ��ȣ�%d\n", (int)_sock, header->dataLength);

		}break;
		}
	}
	//��������
	int SendData(DataHeader* header) {
		if (isRun() && header) {
			return send(_sock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
private:

};



#endif // !Ea
