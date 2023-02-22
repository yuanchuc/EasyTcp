#define FD_SETSIZE 1024
#include"EasyTcpServer.hpp"
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
    EasyTcpServer server;
    server.InitSocket();
    server.Bind(nullptr, 4567);
    server.Listen(15);
    
    std::thread t1(cmdThread);
    t1.detach();    //分离主线程，防止主线程退出后，子线程仍未退出导致问题
    
    while (g_bRun) {
        server.OnRun(); 
    }
    server.Close();
    printf("已退出,任务结束\n");
    getchar();
    return 0;
}
