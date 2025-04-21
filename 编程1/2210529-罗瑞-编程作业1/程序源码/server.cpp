#include <iostream>
#include <fstream>  // ������־�ļ�
#include <winsock2.h>  
#include <thread>   // ���ڹ����˳�������߳�
#pragma comment(lib, "ws2_32.lib") 
#pragma warning(disable:4996)

using namespace std;

// �ͻ��˽ṹ��
struct CLIENT {
    SOCKET client;            // �ͻ����׽���
    char username[32];        // �û�������󳤶���Ϊ32�ַ�
    char buffer[256];         // ��Ϣ����������󳤶���Ϊ256�ַ�
    int index;                // �û�����
    int IP;                   // �ͻ���IP��ַ
};

// ȫ�ֱ���
CLIENT chat_array[20];  // ���֧��20���ͻ���
int num = 0;            // ��ǰ���ӵĿͻ�������
bool serverRunning = true; // �������Ƿ����еı�־

// ��ȡ��ǰʱ�䲢�����ַ���
void getTime(char* buffer, size_t size) {
    time_t timep;
    time(&timep);
    struct tm timeInfo;
    localtime_s(&timeInfo, &timep);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &timeInfo);
}

// ��¼��־��Ϣ����־�ļ�
void logMessage(const char* message) {
    ofstream logFile("server_log.txt", ios::app); // ׷��ģʽ����־�ļ�
    if (logFile.is_open()) {
        char timeBuffer[20];
        getTime(timeBuffer, sizeof(timeBuffer));
        logFile << timeBuffer << " - " << message << endl;
        logFile.close();
    }
}

// ���������պ�ת����Ϣ
DWORD WINAPI CommunicateThread(LPVOID lparam) {
    CLIENT* Client = (CLIENT*)lparam;
    char revData[128];          // ���յ�����
    char recv_time[20] = { 0 }; // ���յ�ʱ��
    char full_message[256];     // ������Ϣ
    char temp[2] = ":";         // ��Ϣ�ָ���
    char space[2] = " ";        // �ո�
    char left[10] = "�û���";
    char right[5] = "��:";

    while (true) {
        memset(revData, 0, sizeof(revData));
        memset(full_message, 0, sizeof(full_message));

        // ����ʱ�����Ϣ����
        int recv_lent = recv(Client->client, recv_time, 20, 0);
        int ret = recv(Client->client, revData, 128, 0);

        if (ret > 0) {
            // ����˳���Ϣ
            if (strcmp(revData, "\\exit") == 0) {
                // ����˳���Ϣ
                cout << "��" << Client->username << "���˳�������" << endl;
                string exitMessage = "��" + std::string(Client->username) + "���˳�������";
                logMessage(exitMessage.c_str());

                // ֪ͨ�����ͻ���
                for (int i = 0; i < num; i++) {
                    if (i != Client->index) {
                        send(chat_array[i].client, exitMessage.c_str(), exitMessage.length(), 0);
                    }
                }
                break; // �˳��߳�
            }

            if (strncmp(revData, "\\msg ", 5) == 0) { // ����Ƿ��� "\\msg " ��ͷ
             
                string message(revData);
                size_t firstSpacePos = message.find(' '); // �ҵ���һ���ո��λ��
                if (firstSpacePos != string::npos) {
                    size_t secondSpacePos = message.find(' ', firstSpacePos + 1); // �ҵ��ڶ����ո��λ��

                    if (secondSpacePos != string::npos) {
                        string targetUser = message.substr(firstSpacePos + 1, secondSpacePos - firstSpacePos - 1); // ��ȡĿ���û���
                        string privateMessage = message.substr(secondSpacePos + 1); // ��ȡ��Ϣ����

                        // ����������˽����Ϣ
                        string fullMessage = string(recv_time) + " [˽��] " + string(Client->username) + " -> " + targetUser + ": " + privateMessage;

                        // ��ӡ����¼��Ϣ
                        cout << fullMessage << endl;
                        logMessage(fullMessage.c_str());

                        // ����Ŀ���û���������Ϣ
                        bool userFound = false; // ��־���ڼ���û��Ƿ��ҵ�
                        for (int i = 0; i < num; i++) {
                            if (strcmp(chat_array[i].username, targetUser.c_str()) == 0) {
                                // Ŀ���û��ҵ�������˽����Ϣ
                                send(chat_array[i].client, fullMessage.c_str(), fullMessage.length(), 0);
                                userFound = true;
                                break; // Ŀ���û��ҵ����˳�ѭ��
                            }
                        }

                        if (!userFound) {
                            // ���û���ҵ�Ŀ���û��������߷��ʹ�����Ϣ
                            string errorMessage = "�û� " + targetUser + " �����߻򲻴��ڡ�";
                            send(Client->client, errorMessage.c_str(), errorMessage.length(), 0);
                        }
                    }
                }
                continue;
            }





            // ƴ����Ϣ
            strcpy(full_message, recv_time);
            strcat(full_message, space);
            strcat(full_message, left);
            strcat(full_message, Client->username);
            strcat(full_message, right);
            strcat(full_message, revData);

            // ��ӡ����¼��Ϣ
            cout << full_message << endl;
            logMessage(full_message);

            // ����Ϣת���������ͻ���
            for (int i = 0; i < num; i++) {
                if (i != Client->index) {
                    send(chat_array[i].client, full_message, strlen(full_message), 0);
                }
            }
        }
    }

    // �ر��׽��ֲ���¼��־
    closesocket(Client->client);
    return 0;
}



// �������������̺߳���
void checkExitCommand() {
    string command;
    while (serverRunning) {
        cin >> command;
        if (command == "exit") {
            serverRunning = false;
            cout << "�����������˳�..." << endl;
            logMessage("�������˳�");
            exit(0);  // ֱ���˳�����
        }
    }
}

// ����������ʼ�����������������Ӳ����������߳�
int main() {
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        cout << "��ʼ���׽��ֿ�ʧ�ܣ�" << endl;
        return 0;
    }

    SOCKET Srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Srv == INVALID_SOCKET) {
        cout << "�׽��ִ���ʧ��" << endl;
        WSACleanup();
        return 0;
    }

    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(49711);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(Srv, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR) {
        cout << "��ʧ��" << endl;
        closesocket(Srv);
        WSACleanup();
        return -1;
    }

    listen(Srv, 2); // �����˿�

    cout << "��������ʼ�����" << endl;
    cout << "���������ҳɹ��������ȴ��ͻ�����..." << endl;

    logMessage("��������������ʼ�ȴ�����");

    sockaddr_in ClientAddr;
    int len = sizeof(ClientAddr);

    // ��������˳�������߳�
    thread exitThread(checkExitCommand);

    while (serverRunning) {
        chat_array[num].client = accept(Srv, (SOCKADDR*)&ClientAddr, &len);
        if (chat_array[num].client == SOCKET_ERROR) {
            cout << "���տͻ�������ʧ��" << WSAGetLastError() << endl;
            logMessage("�ͻ�������ʧ��");
            return 0;
        }

        chat_array[num].index = num;

        // ���տͻ����û���
        recv(chat_array[num].client, chat_array[num].username, sizeof(chat_array[num].username), 0);

        // ��ȡ��ǰʱ��
        char timeBuffer[20];
        getTime(timeBuffer, sizeof(timeBuffer));

        cout << timeBuffer << " �û���" << chat_array[num].username << "������������" << endl;

        // ��¼�û�������־
        logMessage((string(chat_array[num].username) + " �������ӳɹ�").c_str());

        // �㲥�û�������Ϣ�������ͻ���
        string joinMessage = string(timeBuffer) + " �û���" + string(chat_array[num].username) + "������������";
        for (int i = 0; i < num; i++) {
            if (i != num) {
                send(chat_array[i].client, joinMessage.c_str(), joinMessage.length(), 0);
            }
        }

        DWORD dwThreadId;
        HANDLE hThread;
        hThread = CreateThread(NULL, NULL, CommunicateThread, &chat_array[num], 0, &dwThreadId);

        if (hThread == NULL) {
            cout << "�����߳�ʧ��" << endl;
            logMessage("�̴߳���ʧ��");
        }
        else {
            CloseHandle(hThread);  // �ر��߳̾������Ӱ���߳�����
        }

        num++;
    }


    closesocket(Srv);
    WSACleanup();
   
    exitThread.join();

    cout << "���������˳���" << endl;
    

    L: return 0;
}
