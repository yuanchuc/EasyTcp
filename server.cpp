#define FD_SETSIZE 1024
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

int main() {
    EasyTcpServer server;
    server.InitSocket();
    server.Bind(nullptr, 4567);
    server.Listen(15);
    
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
