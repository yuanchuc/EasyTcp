#include "EasyTcpClient.hpp"
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
    const int nCount = 2000;
    EasyTcpClient* client[nCount];
    for (int i = 0; i < nCount; i++) {
        client[i] = new EasyTcpClient();
        client[i]->Connect("127.0.0.1", 4567);
    }
    
    std::thread t1(cmdThread);
    t1.detach();    //�������̣߳���ֹ���߳��˳������߳���δ�˳��������� 
    Login ret;
    strcpy(ret.userName, "csd");
    strcpy(ret.PassWord, "123456");

    while (g_bRun)
    {
        /*client.OnRun();*/

        for (int i = 0; i < nCount; i++) {
            client[i]->SendData(&ret);
            //client[i]->OnRun();
        }
        //printf("����ʱ�䴦 ������ҵ��\n");
        //Sleep(1000);      
    }
    for (int i = 0; i < nCount; i++) {
        client[i]->Close();
    }

    getchar();
    return 0;
}
