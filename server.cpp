
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
class MyServer:public EasyTcpServer
{
public:
    virtual void OnNetJoin(ClientSocket* pClient) {
        _clientCount++;
        printf("client<%d> join\n", pClient->sockfd());
    }
    //cellServer 4 线程不安全
    virtual void OnNetLeave(ClientSocket* pClient) {
        _clientCount--;
        printf("client<%d> leave\n", pClient->sockfd());
    }
    //cellServer 4 线程不安全
    virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header) {
        _msgCount++;
        switch (header->cmd)
        {
        case CMD_LOGIN: {

            Login* login = (Login*)header;
            //printf("收到客户端<Socket=%d>请求：CMD_LOGIN,数据长度：%d,userName = %s  PassaWord=%s\n", (int)pClient->sockfd(), login->dataLength, login->userName, login->PassWord);
            //忽略判断用户密码是否正确的过程
            LoginResult ret;
            pClient->SendData(&ret);
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
            printf("Socket=<%d>收到未定义消息,数据长度：%d\n", (int)pClient->sockfd(), header->dataLength);
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
    t1.detach();    //分离主线程，防止主线程退出后，子线程仍未退出导致问题
    
    while (g_bRun) {
        server.OnRun(); 
    }
    server.Close();
    printf("已退出,任务结束\n");
    getchar();
    return 0;
}
