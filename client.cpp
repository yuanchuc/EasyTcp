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

//�߳�����
const int tCount = 4;
//�ͻ�������
const int cCount = 10000;
//�ͻ�������
EasyTcpClient* client[cCount];

void sendThread(int id) {
    //4���߳� ID (1-4)
    int c = cCount / tCount;
    int begin = (id - 1) * c;
    int end = id * c;
    
    for (int i = begin; i < end; i++) {
        if (!g_bRun) {
            return;
        }
        client[i] = new EasyTcpClient();
    }
    for (int i = begin; i < end; i++) {
        if (!g_bRun) {
            return;
        }
        client[i]->Connect("127.0.0.1", 4567);
        printf("thread<%d> Connect = %d\n",id , i);
    }
    std::chrono::milliseconds t(5000);
    std::this_thread::sleep_for(t);
    Login ret[10];
    for (int n = 0; n < 10; n++) {
        strcpy(ret[n].userName, "csd");
        strcpy(ret[n].PassWord, "123456");
    }
    const int nLen = sizeof(ret);
    while (g_bRun)
    {
        /*client.OnRun();*/

        for (int i = begin; i < end; i++) {
            client[i]->SendData(ret,sizeof(ret));
            //client[i]->OnRun();
        }
        //printf("����ʱ�䴦 ������ҵ��\n");
        //Sleep(1000);      
    }
    for (int i = begin; i < cCount; i++) {
        client[i]->Close();
    }
}
int main() {
    std::thread t1(cmdThread);
    t1.detach();

    
    //���������߳�
    for (int n = 0; n < tCount; n++) {
        std::thread t(sendThread,n+1);
        t.detach();
    }
    
    
    
    while (g_bRun) {
#ifdef _WIN32
        Sleep(100);
#else
        sleep(100);
#endif
    }
    return 0;
}
