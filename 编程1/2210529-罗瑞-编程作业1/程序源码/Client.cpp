#include <stdio.h>
#include <iostream>
#include <string>
#include <winsock.h>  // 包含Winsock相关头文件
#include <conio.h>
#include <time.h>
#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")  // 链接Winsock库文件

using namespace std;

char nickname[16] = { 0 };  // 存储用户昵称
bool isServerRunning = true; // 用于控制线程循环的布尔变量

// 接收消息的线程
DWORD WINAPI ReceiveMessages(LPVOID lParam) {
    SOCKET clientSocket = (SOCKET)(LPVOID)lParam;
    char receiveBuffer[256] = { 0 };
    int receiveLen = 0;

    while (isServerRunning) {  // 检查服务器运行状态
        memset(receiveBuffer, 0, sizeof(receiveBuffer));  // 清空缓冲区
        receiveLen = recv(clientSocket, receiveBuffer, 256, 0);
        if (receiveLen <= 0) {  // 检查是否接收失败或连接断开
            cout << "接收消息失败!" << endl;
            isServerRunning = false;  // 停止循环
            break;
        }
        else {
            cout << "<群聊>" << receiveBuffer << endl;
        }
    }
    return 0;
}

DWORD WINAPI SendMessages(LPVOID lParam) {
    SOCKET clientSocket = (SOCKET)(LPVOID)lParam;
    char sendBuffer[128] = { 0 };
    char sendTime[20] = { 0 };

    while (isServerRunning) {
        // 获取当前时间
        time_t currentTime = time(0);
        memset(sendBuffer, 0, sizeof(sendBuffer));  // 清空发送缓冲区
        strftime(sendTime, sizeof(sendTime), "%Y-%m-%d %H:%M:%S", localtime(&currentTime));

        // 使用 fgets() 获取用户输入，确保不会溢出
        if (fgets(sendBuffer, sizeof(sendBuffer), stdin) == nullptr) {
            cout << "读取输入失败。" << endl;
            continue;  // 如果读取失败，重新开始循环
        }

        // 移除末尾的换行符 '\n'
        size_t len = strlen(sendBuffer);
        if (len > 0 && sendBuffer[len - 1] == '\n') {
            sendBuffer[len - 1] = '\0';
        }

        // 如果输入的消息长度达到缓冲区上限，提示并继续下一轮输入
        if (strlen(sendBuffer) >= sizeof(sendBuffer)) {
            cout << "输入消息过长，消息长度应小于 " << sizeof(sendBuffer) << " 字符。" << endl;
            continue;
        }

        // 处理命令输入
        if (sendBuffer[0] == '\\') {
            // 判断具体命令
            if (strcmp(sendBuffer, "\\exit") == 0) {
                isServerRunning = false;  // 标记退出
                int timeSent = send(clientSocket, sendTime, 20, 0);
                int messageSent = send(clientSocket, sendBuffer, 128, 0);

                if (messageSent < 0 || timeSent < 0) {
                    cout << "发送失败！" << endl;
                    break;
                }
                cout << "已退出聊天" << endl;
                exit(0);  // 结束进程
            }
            else if (strncmp(sendBuffer, "\\msg ", 5) == 0) {  // 私聊命令处理
                string fullMsg(sendBuffer);
                size_t firstSpace = fullMsg.find(' ');  // 找到第一个空格

                if (firstSpace != string::npos) {
                    size_t secondSpace = fullMsg.find(' ', firstSpace + 1);  // 找到第二个空格

                    if (secondSpace != string::npos) {
                        string targetUser = fullMsg.substr(firstSpace + 1, secondSpace - firstSpace - 1);  // 提取用户名
                        string privateMsg = fullMsg.substr(secondSpace + 1);  // 提取消息内容

                        // 检查是否为空
                        if (targetUser.empty() || privateMsg.empty()) {
                            cout << "私聊格式错误，用户名或消息不能为空。" << endl;
                        }
                        else {
                            // 构造完整私聊消息
                            string fullMessage = string(sendTime) + " [私聊] " + string(nickname) + " -> " + targetUser + ": " + privateMsg;
                            int timeSent = send(clientSocket, sendTime, 20, 0);
                            int messageSent = send(clientSocket, sendBuffer, 128, 0);
                            if (timeSent < 0 || messageSent < 0) {
                                cout << "私聊消息发送失败！" << endl;
                            }
                            else {
                                cout << "私聊消息已发送！" << endl;
                            }
                        }
                    }
                    else {
                        cout << "私聊格式错误，应为 \\msg 用户名 消息。" << endl;
                    }
                }
                else {
                    cout << "私聊格式错误，应为 \\msg 用户名 消息。" << endl;
                }
                continue;  // 继续下一轮输入
            }
            else {
                // 无效命令提示
                cout << "无效命令: " << sendBuffer << "，未发送。" << endl;
                continue;  // 跳过发送
            }
        }

        printf("%s", sendTime);
        cout << "消息已发送" << endl;
        // 发送普通群聊消息
        int timeSent = send(clientSocket, sendTime, 20, 0);
        int messageSent = send(clientSocket, sendBuffer, 128, 0);

        if (messageSent < 0 || timeSent < 0) {
            cout << "发送失败！" << endl;
            break;
        }
    }
    closesocket(clientSocket);  // 关闭套接字
    WSACleanup();  // 清理Winsock资源
    return 0;
}


int main() {
    // 初始化Winsock
    WORD verRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(verRequested, &wsaData);

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // 创建套接字
    if (clientSocket == INVALID_SOCKET) {
        cout << "套接字创建失败";
        WSACleanup();
        return 0;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(49711);
    serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");  // 服务器IP地址

    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "连接服务器失败" << endl;
        cout << WSAGetLastError() << endl;
        return 0;
    }

    cout << "成功连接服务器" << endl;
    cout << "请输入您的昵称加入群聊：";
    gets_s(nickname);  // 获取用户昵称
    send(clientSocket, nickname, sizeof(nickname), 0);  // 发送昵称到服务器

    // 创建接收和发送消息的线程
    HANDLE recvThread = CreateThread(NULL, 0, ReceiveMessages, (LPVOID)clientSocket, 0, NULL);
    HANDLE sendThread = CreateThread(NULL, 0, SendMessages, (LPVOID)clientSocket, 0, NULL);

    WaitForSingleObject(recvThread, INFINITE);
    WaitForSingleObject(sendThread, INFINITE);

    CloseHandle(recvThread);
    CloseHandle(sendThread);

    return 0;
}
