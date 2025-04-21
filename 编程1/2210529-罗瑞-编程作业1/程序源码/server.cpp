#include <iostream>
#include <fstream>  // 用于日志文件
#include <winsock2.h>  
#include <thread>   // 用于管理退出命令的线程
#pragma comment(lib, "ws2_32.lib") 
#pragma warning(disable:4996)

using namespace std;

// 客户端结构体
struct CLIENT {
    SOCKET client;            // 客户端套接字
    char username[32];        // 用户名，最大长度设为32字符
    char buffer[256];         // 信息缓冲区，最大长度设为256字符
    int index;                // 用户索引
    int IP;                   // 客户端IP地址
};

// 全局变量
CLIENT chat_array[20];  // 最多支持20个客户端
int num = 0;            // 当前连接的客户端数量
bool serverRunning = true; // 服务器是否运行的标志

// 获取当前时间并返回字符串
void getTime(char* buffer, size_t size) {
    time_t timep;
    time(&timep);
    struct tm timeInfo;
    localtime_s(&timeInfo, &timep);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &timeInfo);
}

// 记录日志信息到日志文件
void logMessage(const char* message) {
    ofstream logFile("server_log.txt", ios::app); // 追加模式打开日志文件
    if (logFile.is_open()) {
        char timeBuffer[20];
        getTime(timeBuffer, sizeof(timeBuffer));
        logFile << timeBuffer << " - " << message << endl;
        logFile.close();
    }
}

// 服务器接收和转发消息
DWORD WINAPI CommunicateThread(LPVOID lparam) {
    CLIENT* Client = (CLIENT*)lparam;
    char revData[128];          // 接收的数据
    char recv_time[20] = { 0 }; // 接收的时间
    char full_message[256];     // 完整消息
    char temp[2] = ":";         // 消息分隔符
    char space[2] = " ";        // 空格
    char left[10] = "用户【";
    char right[5] = "】:";

    while (true) {
        memset(revData, 0, sizeof(revData));
        memset(full_message, 0, sizeof(full_message));

        // 接收时间和消息内容
        int recv_lent = recv(Client->client, recv_time, 20, 0);
        int ret = recv(Client->client, revData, 128, 0);

        if (ret > 0) {
            // 检测退出消息
            if (strcmp(revData, "\\exit") == 0) {
                // 输出退出消息
                cout << "【" << Client->username << "】退出聊天室" << endl;
                string exitMessage = "【" + std::string(Client->username) + "】退出聊天室";
                logMessage(exitMessage.c_str());

                // 通知其他客户端
                for (int i = 0; i < num; i++) {
                    if (i != Client->index) {
                        send(chat_array[i].client, exitMessage.c_str(), exitMessage.length(), 0);
                    }
                }
                break; // 退出线程
            }

            if (strncmp(revData, "\\msg ", 5) == 0) { // 检查是否以 "\\msg " 开头
             
                string message(revData);
                size_t firstSpacePos = message.find(' '); // 找到第一个空格的位置
                if (firstSpacePos != string::npos) {
                    size_t secondSpacePos = message.find(' ', firstSpacePos + 1); // 找到第二个空格的位置

                    if (secondSpacePos != string::npos) {
                        string targetUser = message.substr(firstSpacePos + 1, secondSpacePos - firstSpacePos - 1); // 获取目标用户名
                        string privateMessage = message.substr(secondSpacePos + 1); // 获取消息内容

                        // 构建完整的私聊消息
                        string fullMessage = string(recv_time) + " [私聊] " + string(Client->username) + " -> " + targetUser + ": " + privateMessage;

                        // 打印并记录消息
                        cout << fullMessage << endl;
                        logMessage(fullMessage.c_str());

                        // 查找目标用户并发送消息
                        bool userFound = false; // 标志用于检测用户是否找到
                        for (int i = 0; i < num; i++) {
                            if (strcmp(chat_array[i].username, targetUser.c_str()) == 0) {
                                // 目标用户找到，发送私聊消息
                                send(chat_array[i].client, fullMessage.c_str(), fullMessage.length(), 0);
                                userFound = true;
                                break; // 目标用户找到后退出循环
                            }
                        }

                        if (!userFound) {
                            // 如果没有找到目标用户，向发送者发送错误信息
                            string errorMessage = "用户 " + targetUser + " 不在线或不存在。";
                            send(Client->client, errorMessage.c_str(), errorMessage.length(), 0);
                        }
                    }
                }
                continue;
            }





            // 拼接消息
            strcpy(full_message, recv_time);
            strcat(full_message, space);
            strcat(full_message, left);
            strcat(full_message, Client->username);
            strcat(full_message, right);
            strcat(full_message, revData);

            // 打印并记录消息
            cout << full_message << endl;
            logMessage(full_message);

            // 将消息转发给其他客户端
            for (int i = 0; i < num; i++) {
                if (i != Client->index) {
                    send(chat_array[i].client, full_message, strlen(full_message), 0);
                }
            }
        }
    }

    // 关闭套接字并记录日志
    closesocket(Client->client);
    return 0;
}



// 检查输入命令的线程函数
void checkExitCommand() {
    string command;
    while (serverRunning) {
        cin >> command;
        if (command == "exit") {
            serverRunning = false;
            cout << "服务器即将退出..." << endl;
            logMessage("服务器退出");
            exit(0);  // 直接退出程序
        }
    }
}

// 主函数：初始化服务器，监听连接并创建处理线程
int main() {
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        cout << "初始化套接字库失败！" << endl;
        return 0;
    }

    SOCKET Srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Srv == INVALID_SOCKET) {
        cout << "套接字创建失败" << endl;
        WSACleanup();
        return 0;
    }

    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(49711);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(Srv, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR) {
        cout << "绑定失败" << endl;
        closesocket(Srv);
        WSACleanup();
        return -1;
    }

    listen(Srv, 2); // 监听端口

    cout << "服务器初始化完成" << endl;
    cout << "多人聊天室成功开启，等待客户连接..." << endl;

    logMessage("服务器启动并开始等待连接");

    sockaddr_in ClientAddr;
    int len = sizeof(ClientAddr);

    // 启动检查退出命令的线程
    thread exitThread(checkExitCommand);

    while (serverRunning) {
        chat_array[num].client = accept(Srv, (SOCKADDR*)&ClientAddr, &len);
        if (chat_array[num].client == SOCKET_ERROR) {
            cout << "接收客户端请求失败" << WSAGetLastError() << endl;
            logMessage("客户端连接失败");
            return 0;
        }

        chat_array[num].index = num;

        // 接收客户端用户名
        recv(chat_array[num].client, chat_array[num].username, sizeof(chat_array[num].username), 0);

        // 获取当前时间
        char timeBuffer[20];
        getTime(timeBuffer, sizeof(timeBuffer));

        cout << timeBuffer << " 用户【" << chat_array[num].username << "】加入聊天室" << endl;

        // 记录用户加入日志
        logMessage((string(chat_array[num].username) + " 建立连接成功").c_str());

        // 广播用户加入消息给其他客户端
        string joinMessage = string(timeBuffer) + " 用户【" + string(chat_array[num].username) + "】加入聊天室";
        for (int i = 0; i < num; i++) {
            if (i != num) {
                send(chat_array[i].client, joinMessage.c_str(), joinMessage.length(), 0);
            }
        }

        DWORD dwThreadId;
        HANDLE hThread;
        hThread = CreateThread(NULL, NULL, CommunicateThread, &chat_array[num], 0, &dwThreadId);

        if (hThread == NULL) {
            cout << "创建线程失败" << endl;
            logMessage("线程创建失败");
        }
        else {
            CloseHandle(hThread);  // 关闭线程句柄，不影响线程运行
        }

        num++;
    }


    closesocket(Srv);
    WSACleanup();
   
    exitThread.join();

    cout << "服务器已退出。" << endl;
    

    L: return 0;
}
