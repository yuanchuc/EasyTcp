#include "EasyTcpClient.hpp"
#include<thread>

bool g_bRun = true;

void cmdThread() {
    //3输入请求命令
    while (true)
    {
        char cmdBuf[256] = {};
        scanf("%s", cmdBuf);
        //4处理请求
        if (0 == strcmp(cmdBuf, "exit")) {
            g_bRun = false;
            printf("收到exit命令，任务结束。。\n");
            return;
        }
        else {
            printf("不支持的命令，请重新输入\n");
        }
    }
}

//线程数量
const int tCount = 4;
//客户端数量
const int cCount = 10000;
//客户端数组
EasyTcpClient* client[cCount];

void sendThread(int id) {
    //4个线程 ID (1-4)
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
        //printf("空闲时间处 理其他业务\n");
        //Sleep(1000);      
    }
    for (int i = begin; i < cCount; i++) {
        client[i]->Close();
    }
}
int main() {
    std::thread t1(cmdThread);
    t1.detach();

    
    //启动发送线程
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
