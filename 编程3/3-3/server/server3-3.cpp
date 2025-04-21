#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SERVER_FILE_PATH "D:/28301/Desktop/get/" // server�ļ�����λ��
// ȫ�ֱ���
// ����ƴ���������ļ�·��
char FileName[100];

//====================================================================================
//���� ������
//2022��
//====================================================================================



#include <cstdlib> // ���� rand() �� srand()
#include <ctime>   // ���� time()
#include <WinSock2.h>
#include <iostream>
#include <thread>
#include <string>
#include <time.h>
#include <fstream>
#include <sstream>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

//�궨��˿��Լ�IP
#define SERVER_ip "127.0.0.1"
#define CLIENT_ip "127.0.0.1"
#define SERVER_port 10001      // �������˿�
#define CLIENT_port 40001      // �ͻ��˶˿�
SOCKADDR_IN server, client; //ȫ�ֶ������˺Ϳͻ��˵�SOCKADDR_IN�������鷳�����ô���
SOCKET sock; // �׽���
int l = sizeof(SOCKADDR);

//һ�����֧�ִ���0.5GB��С���ļ�
#define UDP_DATAGRAM_SIZE (8 * 1024) //UDP�������ݴ�С����(unsigned short���ͱ������ɿ���)
#define pack unsigned short  //���֧��65536�����ݰ�

//�궨�廬�����ڴ�С����ʱ����
//��С��26��UDP���ݱ�
#define windowlength 25
#define timelimit 100 //100ms
//ȫ�ֱ�����������������ֹλ�á��������кš�udp���ݱ������Լ����һ�����ݱ����ֽ���
int windowtop = 0;
int windowbase = 0;
int sendseq = 0;
pack udp_num;
pack rest;




//�ļ����ջ����������֧�ַ�Χ�ڿ����õ��Ļ���ռ�
char buffer[65536][UDP_DATAGRAM_SIZE];
//��ʼ�����ݱ������ȷ������,

//�ۻ�ȷ�ϼ�¼����
bool acked[65536] = { false };
//�ۻ�ȷ��ָ��
int accumulate_point = -1;

using namespace std;

//������IP��ʽת��Ϊ����
unsigned int stringtoint(const std::string& ipAddress) {
    std::istringstream iss(ipAddress);
    std::string token;
    unsigned int result = 0;
    int shift = 24;

    while (std::getline(iss, token, '.')) {
        result |= (std::stoi(token) << shift);
        shift -= 8;
    }
    return result;
}

//UDPα�ײ���(4+4+ 1+1(�ϲ�) +2)
struct pseudoheader {
public:
    unsigned int source_ip;
    unsigned int destination_ip;
    unsigned short con;
    unsigned short length;
};


//UDP���ݱ���
class UDP_DATAGRAM {

    //UDP�ײ�
    unsigned short source_port;
    unsigned short destination_port;
    unsigned short length;
    //У��ͣ���С2�ֽڣ���Ҫ4��16����λ
    unsigned short checksum;

    //UDP���ݣ�Ϊ�˱�֤У��͵��������㣬����Ϊż�����ֽڣ������������ֽڣ��Զ���ĳλ��һ��ȫ���ֽڣ�
    char message[UDP_DATAGRAM_SIZE];

    //��֤�ɿ�����Э��ʵ�ֵı���
    unsigned short state;//״̬
    unsigned short seq, ack;//���кź�ȷ�Ϻ�


public:
    void showseq() { cout << "\n��������seqΪ" << this->seq << endl; }
    //����ack
    void setack(unsigned short ack) { this->ack = ack; }
    //��ȡack
    unsigned short getack() { return this->ack; }
    //����seq
    void setseq(unsigned short seq) { this->seq = seq; }
    //��ȡseq
    unsigned short getseq() { return this->seq; }
    //����state
    void setstate(unsigned short state) { this->state = state | this->state; }
    //��ȡstate
    unsigned short getstate() { return this->state; }
    //��ȡ���ݵ�ַ
    char* getmessage() { return this->message; }
    //����UDP����
    void setlength(unsigned short length) { this->length = length; }
    //��ȡUDP����
    unsigned short getlength() { return this->length; }
    //����UDPУ��λ
    void calculate_checksum() {
        //����������������sum����Ҫ�������ֽ�
        unsigned int sum = 0;
        //�ڴ˺��������port�ֶ���α�ײ��ֶγ�ʼ��
        this->source_port = CLIENT_port;
        this->destination_port = SERVER_port;
        struct pseudoheader* udp_header = new pseudoheader();
        udp_header->source_ip = stringtoint(CLIENT_ip);
        udp_header->destination_ip = stringtoint(SERVER_ip);
        udp_header->con = 17;
        udp_header->length = this->getlength();
        //���(���ֽڶ������)

        // ���ֶ��ۼ�α�ײ�(���ֽڶ������)
        sum += (udp_header->source_ip >> 16) & 0xFFFF;
        sum += udp_header->source_ip & 0xFFFF;

        sum += (udp_header->destination_ip >> 16) & 0xFFFF;
        sum += udp_header->destination_ip & 0xFFFF;

        sum += udp_header->con;
        sum += udp_header->length;

        //���ֶ��ۼ� UDP �����ײ��Լ�����(���ֽڶ������)
        unsigned short* point = (unsigned short*)this;
        int udp_header_length = sizeof(*this) / 2;
        for (int i = 0; i < udp_header_length; i++) {
            sum += *point++;
        }

        // �������
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        // ȡ��
        this->checksum = ~((unsigned short)sum);

    };
    //У��UDPУ��λ
    bool check_checksum() {
        //����������������sum����Ҫ�������ֽ�
        unsigned int sum = 0;
        //�ڴ˺�������ȻҪ����α�ײ��ֶ�
        struct pseudoheader* udp_header = new pseudoheader();
        udp_header->source_ip = stringtoint(CLIENT_ip);
        udp_header->destination_ip = stringtoint(SERVER_ip);
        udp_header->con = 17;
        udp_header->length = this->getlength();
        //���(���ֽڶ������)

        // ���ֶ��ۼ�α�ײ�(���ֽڶ������)
        sum += (udp_header->source_ip >> 16) & 0xFFFF;
        sum += udp_header->source_ip & 0xFFFF;

        sum += (udp_header->destination_ip >> 16) & 0xFFFF;
        sum += udp_header->destination_ip & 0xFFFF;

        sum += udp_header->con;
        sum += udp_header->length;

        //���ֶ��ۼ� UDP �����ײ��Լ�����(���ֽڶ������)
        unsigned short* point = (unsigned short*)this;
        int udp_header_length = sizeof(*this) / 2;
        for (int i = 0; i < udp_header_length; i++) {
            sum += *point++;
        }

        // �������
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        // ����Ƿ�ȫ1
        if (sum == 0xFFFF)
        {
            cout << "У���Ϊ" << this->checksum << endl;
            printf("%s", "UDPУ����ȷ");
            return true;
        }
        return false;
    }
};

//����״̬�궨�壬��������Ϊunsigned short ���͵�״̬����state
// �����������������ֵĽ�������
// SYN ���������־ 
// SYN & ACK �����ظ�ȷ��
// ACK ����ȷ��

// �˳�����ֻ��Ҫ���λ��ּ���ȷ����������
// FIN ���������־
// FIN &ACK �����ظ�ȷ��
// ACK ���� 

// SYN & REQ ͷ�ļ���־(�����ļ��������Ϣ)
// ACK�����ļ����ݱ�ȷ�ϱ�־
#define SYN 1
#define ACK 2
#define FIN 4
#define REQ 8

static int countcut = 0;


//�����������������Ͷ��������������������ֺ͡����λ��֡�ԭ��
int connection() {
    UDP_DATAGRAM build_first, build_second, build_third;
    //======================================================================================
    // �յ���һ������
    cout << "�����У�******";
    while (true) {
        recvfrom(sock, (char*)&build_first, sizeof(build_first), 0, (SOCKADDR*)&client, &l);
        if (build_first.getstate() & SYN)
            break;
    }

    //======================================================================================
    //���͵ڶ�������
    sendseq++;
    build_second.setstate(SYN);
    build_second.setstate(ACK);
    build_second.setseq(sendseq);
    build_second.setack(build_first.getseq() + 1);
    if (sendto(sock, (char*)&build_second, sizeof(build_second), 0, (SOCKADDR*)&client, l) == SOCKET_ERROR) {
        cout << "�ڶ������ֽ������ķ��ʹ��󣬴�����: " << WSAGetLastError() << endl;
        return 0;
    }
    else {
        cout << "�ڶ������ֽ������ķ��ͳɹ�";
    }
    //======================================================================================
    // �յ�����������
    while (true) {
        cout << "***";
        recvfrom(sock, (char*)&build_third, sizeof(build_third), 0, (SOCKADDR*)&client, &l);
        if (build_third.getstate() & ACK)
            break;
    }
    return 1;
}


int receiveandack() {
    UDP_DATAGRAM recv;
    while (true) {
        memset(&recv, 0, sizeof(UDP_DATAGRAM));
        //Sleep(50);
        recvfrom(sock, (char*)&recv, sizeof(recv), 0, (SOCKADDR*)&client, &l);
        //============================================================================   
           //�棺�������
            //�����������Ϊ��ǰʱ��
        countcut++;
        // ���ɷ�ΧΪ 1 �� 10000 �������
        int a = countcut % 100;//��������ʣ�1��
        if (!a)
            continue;//���������������ģ�ⶪ��
        //==============================================================================
        if (recv.getstate() & SYN && (!(recv.getstate() & FIN)) && recv.check_checksum())
        {
            acked[recv.getseq()] = true;
            int accumulate_point_now = accumulate_point;
            for (int k = accumulate_point + 1; k <= recv.getseq(); k++)
            {
                if (acked[k])
                    accumulate_point_now++;
                else
                    break;
            }
            cout << "�յ����Ϊ" << recv.getseq() << "�ı���," << endl;
//============================================================================================================
//      //���ش��ķ������� ������Reno�㷨������ȷ�ϵı��Ľ��лظ���ʧ�������ͻ��ظ��ػظ����һ��ȷ�ϵ�ack
            //�ظ�ȷ��
            UDP_DATAGRAM sen;
            memset(&sen, 0, sizeof(UDP_DATAGRAM));
            sen.setack(accumulate_point_now);
            sen.setstate(ACK);
            sendto(sock, (char*)&sen, sizeof(sen), 0, (SOCKADDR*)&client, l);
            if (accumulate_point == accumulate_point_now)
            {
                cout << endl;
                cout << "���Ľ���ʧ�򣡶����к�" << accumulate_point_now << "�ظ�ȷ�ϣ�" << endl;
                cout << "==============================================================" << endl;
            }
            else
            {
                cout << endl;
                cout << "�ظ����к�Ϊ" << accumulate_point_now << "���ۻ�ȷ�ϣ�" << endl;
                cout << "==============================================================" << endl;
            }
            
            
            accumulate_point = accumulate_point_now;


                //============================================================================================
            if (recv.getseq() == 0)
            {

                // ���ճ�ʼ���ļ����������ģ������ļ�����Ϣ��UDP������Ϣ��
                // �������յ��ı��ģ���ȡ udp_num ���ļ���
                // ���ڴ���ļ���
                char extractedFileName[100];

                char fileInfo[1024];
                memset(fileInfo, 0, sizeof(fileInfo));
                strcpy(fileInfo, recv.getmessage());


                sscanf(fileInfo, "%d %s %d", &udp_num, extractedFileName, &rest);
                strcpy(FileName, SERVER_FILE_PATH);
                strcat(FileName, extractedFileName);





            }
            else {
                memcpy(buffer[recv.getseq()], recv.getmessage(), recv.getlength());
            }




        }


        else if (recv.getstate() & SYN && (!(recv.getstate() & FIN))) {
            UDP_DATAGRAM sen;
            memset(&sen, 0, sizeof(UDP_DATAGRAM));
            sen.setack(recv.getseq());
            sen.setstate(REQ);
            sendto(sock, (char*)&sen, sizeof(sen), 0, (SOCKADDR*)&client, l);
        }
        else {
            cout << "*";
            //���յ��ļ�����ȷ�ϱ���
            UDP_DATAGRAM sen;
            memset(&sen, 0, sizeof(UDP_DATAGRAM));
            sen.setstate(ACK);
            sen.setstate(FIN);
            sendto(sock, (char*)&sen, sizeof(sen), 0, (SOCKADDR*)&client, l);

            break;
        }
    }
    return 1;
}

int writedata() {
    ofstream out;
    out.open(FileName, std::ios::out | std::ios::binary);
    for (int i = 1; i < udp_num - 1; i++)
        out.write(buffer[i], UDP_DATAGRAM_SIZE);


    out.write(buffer[udp_num - 1], rest);
    out.close();
    out.clear();
    return 1;
}
int disconnection() {
    return 1;
}
int main() {
    WSADATA WSAData;
    WORD version = MAKEWORD(2, 2);
    if (0 != WSAStartup(version, &WSAData))
    {
        printf("%s", "[error]WSAStartup����!\n");
        return 0;
    }

    client.sin_addr.S_un.S_addr = inet_addr(CLIENT_ip);
    client.sin_family = AF_INET;
    client.sin_port = htons(CLIENT_port);// ���÷������˿ں�


    server.sin_addr.S_un.S_addr = inet_addr(SERVER_ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_port);
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    u_long mode = 0;  // 1 ��ʾ������ģʽ
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        cout << "[error] �޷���������ģʽ, ������: " << WSAGetLastError() << endl;
        closesocket(sock);
        WSACleanup();
        return -1;
    }
    bind(sock, (SOCKADDR*)&server, sizeof(SOCKADDR));
    if (sock == INVALID_SOCKET) {
        cout << "�׽��ִ���ʧ��" << endl;
        return 0;
    }

    if (connection())
        cout << "�����ɹ�!" << endl;
    else
        cout << "����ʧ�ܣ�" << endl;

    if (receiveandack())
        cout << "�ļ����ճɹ���" << endl;

    //���λ���
    if (disconnection())
        cout << "�����ɹ�" << endl;

    if (writedata())
        cout << "д�ļ���ɣ�" << endl;
    else
        cout << "д�ļ�ʧ�ܣ�" << endl;




    return 0;
}