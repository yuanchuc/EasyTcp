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
int main() {
    const int nCount = 2000;
    EasyTcpClient* client[nCount];
    for (int i = 0; i < nCount; i++) {
        client[i] = new EasyTcpClient();
        client[i]->Connect("127.0.0.1", 4567);
    }
    
    std::thread t1(cmdThread);
    t1.detach();    //分离主线程，防止主线程退出后，子线程仍未退出导致问题 
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
        //printf("空闲时间处 理其他业务\n");
        //Sleep(1000);      
    }
    for (int i = 0; i < nCount; i++) {
        client[i]->Close();
    }

    getchar();
    return 0;
}
