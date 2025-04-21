#include <stdio.h>
#include <iostream>
#include <string>
#include <winsock.h>  // ����Winsock���ͷ�ļ�
#include <conio.h>
#include <time.h>
#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")  // ����Winsock���ļ�

using namespace std;

char nickname[16] = { 0 };  // �洢�û��ǳ�
bool isServerRunning = true; // ���ڿ����߳�ѭ���Ĳ�������

// ������Ϣ���߳�
DWORD WINAPI ReceiveMessages(LPVOID lParam) {
    SOCKET clientSocket = (SOCKET)(LPVOID)lParam;
    char receiveBuffer[256] = { 0 };
    int receiveLen = 0;

    while (isServerRunning) {  // ������������״̬
        memset(receiveBuffer, 0, sizeof(receiveBuffer));  // ��ջ�����
        receiveLen = recv(clientSocket, receiveBuffer, 256, 0);
        if (receiveLen <= 0) {  // ����Ƿ����ʧ�ܻ����ӶϿ�
            cout << "������Ϣʧ��!" << endl;
            isServerRunning = false;  // ֹͣѭ��
            break;
        }
        else {
            cout << "<Ⱥ��>" << receiveBuffer << endl;
        }
    }
    return 0;
}

DWORD WINAPI SendMessages(LPVOID lParam) {
    SOCKET clientSocket = (SOCKET)(LPVOID)lParam;
    char sendBuffer[128] = { 0 };
    char sendTime[20] = { 0 };

    while (isServerRunning) {
        // ��ȡ��ǰʱ��
        time_t currentTime = time(0);
        memset(sendBuffer, 0, sizeof(sendBuffer));  // ��շ��ͻ�����
        strftime(sendTime, sizeof(sendTime), "%Y-%m-%d %H:%M:%S", localtime(&currentTime));

        // ʹ�� fgets() ��ȡ�û����룬ȷ���������
        if (fgets(sendBuffer, sizeof(sendBuffer), stdin) == nullptr) {
            cout << "��ȡ����ʧ�ܡ�" << endl;
            continue;  // �����ȡʧ�ܣ����¿�ʼѭ��
        }

        // �Ƴ�ĩβ�Ļ��з� '\n'
        size_t len = strlen(sendBuffer);
        if (len > 0 && sendBuffer[len - 1] == '\n') {
            sendBuffer[len - 1] = '\0';
        }

        // ����������Ϣ���ȴﵽ���������ޣ���ʾ��������һ������
        if (strlen(sendBuffer) >= sizeof(sendBuffer)) {
            cout << "������Ϣ��������Ϣ����ӦС�� " << sizeof(sendBuffer) << " �ַ���" << endl;
            continue;
        }

        // ������������
        if (sendBuffer[0] == '\\') {
            // �жϾ�������
            if (strcmp(sendBuffer, "\\exit") == 0) {
                isServerRunning = false;  // ����˳�
                int timeSent = send(clientSocket, sendTime, 20, 0);
                int messageSent = send(clientSocket, sendBuffer, 128, 0);

                if (messageSent < 0 || timeSent < 0) {
                    cout << "����ʧ�ܣ�" << endl;
                    break;
                }
                cout << "���˳�����" << endl;
                exit(0);  // ��������
            }
            else if (strncmp(sendBuffer, "\\msg ", 5) == 0) {  // ˽�������
                string fullMsg(sendBuffer);
                size_t firstSpace = fullMsg.find(' ');  // �ҵ���һ���ո�

                if (firstSpace != string::npos) {
                    size_t secondSpace = fullMsg.find(' ', firstSpace + 1);  // �ҵ��ڶ����ո�

                    if (secondSpace != string::npos) {
                        string targetUser = fullMsg.substr(firstSpace + 1, secondSpace - firstSpace - 1);  // ��ȡ�û���
                        string privateMsg = fullMsg.substr(secondSpace + 1);  // ��ȡ��Ϣ����

                        // ����Ƿ�Ϊ��
                        if (targetUser.empty() || privateMsg.empty()) {
                            cout << "˽�ĸ�ʽ�����û�������Ϣ����Ϊ�ա�" << endl;
                        }
                        else {
                            // ��������˽����Ϣ
                            string fullMessage = string(sendTime) + " [˽��] " + string(nickname) + " -> " + targetUser + ": " + privateMsg;
                            int timeSent = send(clientSocket, sendTime, 20, 0);
                            int messageSent = send(clientSocket, sendBuffer, 128, 0);
                            if (timeSent < 0 || messageSent < 0) {
                                cout << "˽����Ϣ����ʧ�ܣ�" << endl;
                            }
                            else {
                                cout << "˽����Ϣ�ѷ��ͣ�" << endl;
                            }
                        }
                    }
                    else {
                        cout << "˽�ĸ�ʽ����ӦΪ \\msg �û��� ��Ϣ��" << endl;
                    }
                }
                else {
                    cout << "˽�ĸ�ʽ����ӦΪ \\msg �û��� ��Ϣ��" << endl;
                }
                continue;  // ������һ������
            }
            else {
                // ��Ч������ʾ
                cout << "��Ч����: " << sendBuffer << "��δ���͡�" << endl;
                continue;  // ��������
            }
        }

        printf("%s", sendTime);
        cout << "��Ϣ�ѷ���" << endl;
        // ������ͨȺ����Ϣ
        int timeSent = send(clientSocket, sendTime, 20, 0);
        int messageSent = send(clientSocket, sendBuffer, 128, 0);

        if (messageSent < 0 || timeSent < 0) {
            cout << "����ʧ�ܣ�" << endl;
            break;
        }
    }
    closesocket(clientSocket);  // �ر��׽���
    WSACleanup();  // ����Winsock��Դ
    return 0;
}


int main() {
    // ��ʼ��Winsock
    WORD verRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(verRequested, &wsaData);

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // �����׽���
    if (clientSocket == INVALID_SOCKET) {
        cout << "�׽��ִ���ʧ��";
        WSACleanup();
        return 0;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(49711);
    serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");  // ������IP��ַ

    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "���ӷ�����ʧ��" << endl;
        cout << WSAGetLastError() << endl;
        return 0;
    }

    cout << "�ɹ����ӷ�����" << endl;
    cout << "�����������ǳƼ���Ⱥ�ģ�";
    gets_s(nickname);  // ��ȡ�û��ǳ�
    send(clientSocket, nickname, sizeof(nickname), 0);  // �����ǳƵ�������

    // �������պͷ�����Ϣ���߳�
    HANDLE recvThread = CreateThread(NULL, 0, ReceiveMessages, (LPVOID)clientSocket, 0, NULL);
    HANDLE sendThread = CreateThread(NULL, 0, SendMessages, (LPVOID)clientSocket, 0, NULL);

    WaitForSingleObject(recvThread, INFINITE);
    WaitForSingleObject(sendThread, INFINITE);

    CloseHandle(recvThread);
    CloseHandle(sendThread);

    return 0;
}
