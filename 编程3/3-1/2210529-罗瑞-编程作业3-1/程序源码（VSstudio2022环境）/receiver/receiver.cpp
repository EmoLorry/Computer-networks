#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#include <time.h>
#include <WinSock2.h>
#include <Windows.h>
#include <fstream>
#include <io.h>
#pragma comment(lib, "ws2_32.lib")
#include <string>
#include <iostream>
#include "function.h"
using namespace std;

#define RESENT_TIMES_LIMIT 3                 // �ش�����
#define TIME_LIMIT 10                         // ��ʱ
#define SERVER_FILE_PATH "D:/28301/Desktop/get/" // server�ļ�����λ��

int status;//�����Ƿ�������Ч����
LARGE_INTEGER frequency; //��ʱ��Ƶ��


//һ�����֧�ִ���0.5GB��С���ļ�
#define UDP_DATAGRAM_SIZE (8 * 1024) //UDP�������ݴ�С����(unsigned short���ͱ������ɿ���)

//����ͨ�ŵ�IP�Ͷ˿ں�
std::string IPServer = "127.0.0.1";
std::string IPClient = "127.0.0.1";
u_short SERVERPORT = 20000;
u_short CLIENTPORT = 40000;


//�ĸ�״̬λ�Ķ���,�ò�ͬ��״̬��϶���������λ��־������ʾ��
// ʵ��������2�ֽڴ�С��unsigned short���ͱ���state����¼
#define SYN 0x01
#define ACK 0x02
#define FIN 0x04
#define REQ 0x08

//UDPα�ײ�(4+4+ 1+1(�ϲ�) +2) ��
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
    void showseq() {std::cout << "\n��������seqΪ" << this->seq << endl; }
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
        this->source_port = CLIENTPORT;
        this->destination_port = SERVERPORT;
        struct pseudoheader* udp_header = new pseudoheader();
        udp_header->source_ip = stringtoint(IPClient);
        udp_header->destination_ip = stringtoint(IPServer);
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
        udp_header->source_ip = stringtoint(IPClient);
        udp_header->destination_ip = stringtoint(IPServer);
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
            printf("%s", "У����ȷ\n");
            return true;
        }
        printf("%s", "У�����\n");
        cout << sum;
        return false;
    }
};


int sendDatagram(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& addr)
{
    std::cout << "SENDING\n";
    datagram.calculate_checksum();
    int res = sendto(sock, (char*)&datagram, sizeof(UDP_DATAGRAM), 0, (struct sockaddr*)&addr, sizeof(sockaddr));
    return res;
}

static int lost = 0;
static int sendseq = 1;


int recvDatagram(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& addr)
{
    int res = 0;
    int len = sizeof(SOCKADDR);
    lost++;
    res = recvfrom(sock, (char*)&datagram, sizeof(UDP_DATAGRAM), 0, (struct sockaddr*)&addr, &len);
    //����ԭ�����ò��
    if (lost % 5 == 0)
        memset(&datagram, 0, sizeof(UDP_DATAGRAM));
   
    return res;
}


/* ͣ��Э��SEND */
bool sendAndAck(UDP_DATAGRAM& datagram, UDP_DATAGRAM recv_datagram, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    sendDatagram(datagram, sock, ClientAddr);
    datagram.showseq();
    LARGE_INTEGER clockstart, clockend;
    QueryPerformanceCounter(&clockstart);
    while (1)
    {
        recvDatagram(recv_datagram, sock, ServerAddr);
        if (recv_datagram.getack() == datagram.getseq())
        {
            return 1;
        }
    }
    return 0;
}


/* ͣ��Э�� RECV */
bool RCACK(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& ClientAddr) {
    UDP_DATAGRAM ack;
    memset(&ack, 0, sizeof(UDP_DATAGRAM));

    while (true) {
        // �������ݱ���
        recvDatagram(datagram, sock, ClientAddr);
        std::cout << "Received Seq: " << datagram.getseq() << " | Expected Seq: " << sendseq << std::endl;

        // У��ͼ��
        if (!datagram.check_checksum()) {
            std::cout << "[Error] У��ʹ���\n";

            // ���������ش���ACK����
            ack.setstate(REQ);
            ack.setseq(datagram.getseq());
            sendDatagram(ack, sock, ClientAddr);

            std::cout << "���������ش���ACK���ģ����: ";
            ack.showseq();
            std::cout << "���ݱ�����: " << datagram.getlength() << std::endl;

            // ��ձ�������
            memset(&ack, 0, sizeof(UDP_DATAGRAM));
            memset(&datagram, 0, sizeof(UDP_DATAGRAM));
            continue; // �����ȴ�����
        }

        // ������к��Ƿ�ƥ��
        if (datagram.getseq() == sendseq) {
            std::cout << "[Info] ���к�ƥ��\n";

            // ����ȷ��ACK����
            ack.setack(datagram.getseq());
            ack.setstate(ACK);
            ack.setseq(sendseq);
            sendDatagram(ack, sock, ClientAddr);

            // ���·������кŲ����سɹ�
            sendseq++;
            std::cout << "����ACKȷ�ϱ��ģ����: ";
            ack.showseq();
            return true;
        }

        // ���кŲ�ͬ�������
        std::cout << "[Error] ���кŲ�ͬ������ǰ�������: " << datagram.getseq()
            << "���������: " << sendseq << std::endl;
        break; // �˳�ѭ��
    }

    return false; // ����ʧ��
}

/* �����ļ� */
int recvFile(UDP_DATAGRAM datagram, SOCKET& sock, SOCKADDR_IN& ClientAddr, SOCKADDR_IN& ServerAddr)
{
    // ���ڴ���ļ�·��
    char fileName[100];
    // ���ճ�ʼ���ļ����������ģ������ļ�����Ϣ��UDP������Ϣ��
    // �������յ��ı��ģ���ȡ total ���ļ���
    char fileInfo[1024];
    memset(fileInfo, 0, sizeof(fileInfo));
    strcpy(fileInfo, datagram.getmessage());

    // ���� total �� fileName ��rest
    int total = 0;
    int rest = 0;
    char extractedFileName[100];
    sscanf(fileInfo, "%d %s %d", &total, extractedFileName,&rest);


    // ƴ���������ļ�·��
    strcpy(fileName, SERVER_FILE_PATH);
    strcat(fileName, extractedFileName);

    
    UDP_DATAGRAM back;
    memset(&back, 0, sizeof(UDP_DATAGRAM));
    back.setack(datagram.getseq());
    back.setseq(++sendseq);
    //seq=5
    sendDatagram(back, sock, ClientAddr);
    cout << "��ȷ���ļ�������" << endl;
    LARGE_INTEGER clockstart, clockend;
    QueryPerformanceCounter(&clockstart);

    unsigned int whole=0;
    // �����������ĸ������������Ĵ�С
    if (rest == 0)
        whole = total;
    else
        whole = total - 1;

    int size = UDP_DATAGRAM_SIZE * whole + rest;
    cout <<"�������ֽڴ�СΪ" << size<<endl;
    //��ʼ��Ҫ������ݵ��ַ���
    char* recvbuf = (char*)malloc(size);

    for (int i = 0; i < whole; i++)
    {
        cout << "���ձ����ļ���" <<i<< endl;
        UDP_DATAGRAM data;
        memset(&data, 0, sizeof(UDP_DATAGRAM));
        RCACK(data, sock, ClientAddr);
        memcpy(recvbuf + i * UDP_DATAGRAM_SIZE, data.getmessage(), UDP_DATAGRAM_SIZE);
    }

    //����ʣ�������
    UDP_DATAGRAM data, respond;
    memset(&data, 0, sizeof(UDP_DATAGRAM));
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    cout << "rest ��СΪ " << rest << endl;
    RCACK(data, sock, ClientAddr);
    memcpy(recvbuf + (whole * UDP_DATAGRAM_SIZE), data.getmessage(), rest);

    //д���ļ�
    FILE* file;
    file = fopen(fileName, "wb");
    if (recvbuf != 0)
    {
        fwrite(recvbuf, size, 1, file);
        fclose(file);
    }
    QueryPerformanceCounter(&clockend);
    double duration = (double)(clockend.QuadPart - clockstart.QuadPart) / (double)frequency.QuadPart;
    // ��¼����ʱ���������
    cout << "\n����ʱ��:" << duration << "��\n";
    cout << "������:" << size / duration << "char/s\n----------------------------------\n";
    return 1;
}

/* ���� */
int initateConnect(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& addr)
{
    UDP_DATAGRAM respond;
    UDP_DATAGRAM client_last_respond;
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    ++sendseq;
    //seq=2
    respond.setseq(sendseq);
    respond.setack(datagram.getseq());
    respond.setstate(SYN);
    respond.setstate(ACK);
    
    // ���ͱ���ȷ����������
    sendDatagram(respond, sock, addr);
    respond.showseq();
    int len = sizeof(SOCKADDR);
    //���յ���������
    recvfrom(sock, (char*)&client_last_respond, sizeof(UDP_DATAGRAM), 0, (struct sockaddr*)&addr, &len);
    if (client_last_respond.getstate() & (ACK))
        return 1;
    else
        return 0;
}

/* ���� */
int disConnect(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    // ��ʼ������
    UDP_DATAGRAM data;
    UDP_DATAGRAM respond;
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    memset(&data, 0, sizeof(UDP_DATAGRAM));
    // ���ñ�־λFIN��ACK
    data.setstate(FIN);
    data.setstate(ACK);
    data.setack(datagram.getseq());
    // ���ͱ��Ĳ��ȴ�ȷ��
    if (sendAndAck(data, respond, sock, ServerAddr, ClientAddr))
        return 1;
    else
        return 0;
}

int main()
{
    WORD wVersionRequested;
    WSADATA WSAData;
    wVersionRequested = MAKEWORD(1, 1);
    if (0 != WSAStartup(wVersionRequested, &WSAData))
    {
        cout << "WSA����ʧ��\n";
        return 0;
    }
    //�뱾��IP��
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval timeout;

    SOCKADDR_IN ServerAddr;
    ServerAddr.sin_addr.s_addr = inet_addr(&IPServer[0]);
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(SERVERPORT);
    bind(sock, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));

    //���ն˵�IP��ַ
    SOCKADDR_IN ClientAddr;
    ClientAddr.sin_addr.s_addr = inet_addr(&IPClient[0]);
    ClientAddr.sin_family = AF_INET;
    ClientAddr.sin_port = htons(CLIENTPORT);

    QueryPerformanceFrequency(&frequency);
    while (1)
    {
        UDP_DATAGRAM data;
        memset(&data, 0, sizeof(UDP_DATAGRAM));
        recvDatagram(data, sock, ClientAddr);
        if ((data.getstate() & (SYN)) && !(data.getstate()&(REQ)))
        {
            if (initateConnect(data, sock, ClientAddr))
            {
                sendseq += 2;
                //seq=4
                status = 1;
                cout << "��������\n";
            }
            else
                cout << "���ӽ���ʧ��\n";
        }
        else if (data.getstate()&(FIN))
        {
            if (disConnect(data, sock, ServerAddr, ClientAddr))
            {
                status = 0;
                cout << "�����ɹ�\n";
                //����
                sendseq = 1;
            }
        }
        else if ((data.getstate() & (SYN)) && (data.getstate() & (REQ)))
        {
            if (status)
            {
                recvFile(data, sock, ClientAddr, ServerAddr);
                //����
                sendseq = 1;
            }
        }
    }
    closesocket(sock);
    WSACleanup();
    return 0;
}