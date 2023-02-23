
#include"EasyTcpServer.hpp"
#include<thread>
bool g_bRun = true;
void cmdThread() {
    //3������������
    while (true)
    {
        char cmdBuf[256] = {};
        scanf("%s", cmdBuf);
        //4��������
        if (0 == strcmp(cmdBuf, "exit")) {
            g_bRun = false;
            printf("�յ�exit��������������\n");
            return;
        }
        else {
            printf("��֧�ֵ��������������\n");
        }
    }
}
class MyServer:public EasyTcpServer
{
public:
    virtual void OnNetJoin(ClientSocket* pClient) {
        _clientCount++;
        printf("client<%d> join\n", pClient->sockfd());
    }
    //cellServer 4 �̲߳���ȫ
    virtual void OnNetLeave(ClientSocket* pClient) {
        _clientCount--;
        printf("client<%d> leave\n", pClient->sockfd());
    }
    //cellServer 4 �̲߳���ȫ
    virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header) {
        _msgCount++;
        switch (header->cmd)
        {
        case CMD_LOGIN: {

            Login* login = (Login*)header;
            //printf("�յ��ͻ���<Socket=%d>����CMD_LOGIN,���ݳ��ȣ�%d,userName = %s  PassaWord=%s\n", (int)pClient->sockfd(), login->dataLength, login->userName, login->PassWord);
            //�����ж��û������Ƿ���ȷ�Ĺ���
            LoginResult ret;
            pClient->SendData(&ret);
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
            printf("Socket=<%d>�յ�δ������Ϣ,���ݳ��ȣ�%d\n", (int)pClient->sockfd(), header->dataLength);
            /*DataHeader ret;
            SendData(cSock, &ret); */
        }
               break;
        }
    }
private:

};

int main() {
    MyServer server;
    server.InitSocket();
    server.Bind(nullptr, 4567);
    server.Listen(15);
    server.Start(2);
    std::thread t1(cmdThread);
    t1.detach();    //�������̣߳���ֹ���߳��˳������߳���δ�˳���������
    
    while (g_bRun) {
        server.OnRun(); 
    }
    server.Close();
    printf("���˳�,�������\n");
    getchar();
    return 0;
}
