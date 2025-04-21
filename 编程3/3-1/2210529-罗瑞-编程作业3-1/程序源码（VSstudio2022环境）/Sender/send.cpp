#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#include <winsock2.h>
#include <fstream>
#include <io.h>
#include <time.h>
#include <thread>
#include <sstream>
#include <string>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")
//���߱��������� ws2_32.lib ��̬�⡣
//ws2_32.lib �� Windows ƽ̨�� Winsock �ľ�̬�⣬
//�ṩ������������ĺ������� socket��bind��sendto �ȣ���
using namespace std;

//-------------------------------
//���� 2210529 ����
// 2024.11.20
//-------------------------------

//����ͨ�ŵ�IP�Ͷ˿ں�
std::string IPServer = "127.0.0.1";
std::string IPClient = "127.0.0.1";
unsigned short SERVERPORT = 20000;
unsigned short CLIENTPORT = 40000;

//һ�����֧�ִ���0.5GB��С���ļ�
#define UDP_DATAGRAM_SIZE (8 * 1024) //UDP�������ݴ�С����(unsigned short���ͱ������ɿ���)

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
            printf("%s", "UDPУ����ȷ");
            return true;
        }
        return false;
    }
};

/* SEND */
//���ʹ�����������У��Ͳ����͵�addr��ַ
int sendDatagram(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& addr)
{
    cout << "SENDING\n";
    datagram.calculate_checksum();
    return sendto(sock, (char*)&datagram, sizeof(UDP_DATAGRAM), 0, (struct sockaddr*)&addr, sizeof(sockaddr));
}
//�ĸ�״̬λ�Ķ���,�ò�ͬ��״̬��϶���������λ��־������ʾ��
// ʵ��������2�ֽڴ�С��unsigned short���ͱ���state����¼
//ʹ������߼�����ֱ�Ӱ�λ����
#define SYN 0x01
#define ACK 0x02
#define FIN 0x04
#define REQ 0x08
#define TIMELIMIT 10
#define RESENT_TIMES_LIMIT 3
UDP_DATAGRAM send_DataGram, respond;
UDP_DATAGRAM datagram0;
SOCKET sock0;
SOCKADDR_IN ServerAddr0;
int times0 = 0;
static int nowseq = 1;





//COPY//
/* ��ʱ�ش� */
void handleTimeout(UDP_DATAGRAM& datagram, SOCKET sock, SOCKADDR_IN ServerAddr, LARGE_INTEGER& clockstart, int& times)
{
    times++;
    cout << "�����ش� " << datagram.getseq() << " �� " << times << " ��\n";
    QueryPerformanceCounter(&clockstart);

    sendDatagram(datagram, sock, ServerAddr);
}



// ȫ�ֱ���
static LARGE_INTEGER timeStart, timeEnd, frequency;
static bool isTimeout = false; // ��ʱ��־
static bool isTimerActive = true; // ��ʱ���Ƿ񼤻�
static int elapsedTicks = 1; // ���ڼ�¼����ӡ������ʱ��
static int resendCount = 0; // ��¼�ش�����
static const int TIMEOUT_LIMIT = 5; // ��ʱ�ش����ƴ���

// ��ʱ�����Ʊ�־
static bool isTimerRunning = true;

// ģ����ⲿ��Դ
UDP_DATAGRAM datagram0;
SOCKET sock0;
SOCKADDR_IN ServerAddr0;

// ��ʼ����ʱ��
void initializeTimer() {
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&timeStart);
}

// ��ӡʱ����Ϣ
void printElapsedTime(double elapsedTime) {
    if (elapsedTime > elapsedTicks) {
        std::cout << "Elapsed time: " << elapsedTicks << "s\n";
        elapsedTicks++;
    }
}

// ���ü�ʱ��
void resetTimer() {
    QueryPerformanceCounter(&timeStart);
    elapsedTicks = 1;
}

// ��ʱ���߼�
void timerLogic() {
    while (isTimerRunning) {
        if (!isTimerActive) continue; // �����ʱ��δ��������߼�

        // ��ȡ��ǰʱ��
        QueryPerformanceCounter(&timeEnd);
        double elapsedTime = static_cast<double>(timeEnd.QuadPart - timeStart.QuadPart) / frequency.QuadPart;

        // ��ӡʱ����Ϣ
        printElapsedTime(elapsedTime);

        // ����Ƿ�ʱ
        if (elapsedTime >= TIMELIMIT) {
            isTimeout = true;
            std::cout << "----------------��ʱ----------------" << std::endl;

            // ���ó�ʱ������
            handleTimeout(datagram0, sock0, ServerAddr0, timeStart, resendCount);

            // ���ü�ʱ��
            resetTimer();

            // ����Ƿ�ﵽ�ش�����
            if (++resendCount >= TIMEOUT_LIMIT) {
                std::cout << "----------------�ش�����----------------" << std::endl;
                exit(EXIT_FAILURE); // �����˳�
            }
        }
    }
}


int recvDatagram(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& addr)
{
    int len = sizeof(SOCKADDR);
    int res = recvfrom(sock, (char*)&datagram, sizeof(UDP_DATAGRAM), 0, (struct sockaddr*)&addr, &len);
    cout << "RECVING\n";
    return res;
}

/* ͣ��Э�� */
bool sendAndwait(UDP_DATAGRAM& datagram, UDP_DATAGRAM& recv_datagram, SOCKET sock, SOCKADDR_IN ServerAddr, SOCKADDR_IN ClientAddr)
{
    sendDatagram(datagram, sock, ServerAddr);
    datagram.showseq();
    LARGE_INTEGER clockstart, clockend;
    QueryPerformanceCounter(&clockstart);
    // ���ݱ���
    timeStart = clockstart;
    isTimerActive = 1;
    datagram0 = datagram;
    sock0 = sock;
    ServerAddr0 = ServerAddr;
    times0 = 0;

    printf("%s", "ͣ��Э�顭��\n");
    while (1)
    {
        recvDatagram(recv_datagram, sock, ServerAddr);
        if (recv_datagram.getstate() & REQ)
        {
            printf("����ش�\n");
            sendDatagram(datagram, sock, ServerAddr);
            datagram.showseq();
            QueryPerformanceCounter(&timeStart);
            continue;
        }
        //���ͣ�Ȼظ�
        else if (recv_datagram.getack() == datagram.getseq())
        {
            printf("%s", "��UDP���ķ����ҽ��ճɹ�!!!");
            isTimerActive = 0;
            return 1;
        }
    }
    cout << "[error]����ʧ��\n";
    return 0;
}


/* ��ȡ�ļ� */
char* readFile(char* fileName, unsigned short& whole, unsigned short& rest, unsigned int& size)
{
    // ���ļ�
    ifstream inFile(fileName, ios::in | ios::binary);
    if (!inFile.is_open())
    {
        cout << "[error] �ļ���ʧ��: " << fileName << endl;
        return nullptr;
    }

    // ��ȡ�ļ���С
    inFile.seekg(0, ios::end);
    size = static_cast<unsigned int>(inFile.tellg());
    if (size == 0)
    {
        cout << "[error] �ļ�Ϊ��: " << fileName << endl;
        inFile.close();
        return nullptr;
    }

    // �����ļ���ͷ
    inFile.seekg(0, ios::beg);

    // �����ڴ�
    char* filedata = new (nothrow) char[size];
    if (!filedata)
    {
        cout << "[error] �ڴ����ʧ�ܣ��ļ���С: " << size << " �ֽ�" << endl;
        inFile.close();
        return nullptr;
    }

    // ��ȡ�ļ�����
    inFile.read(filedata, size);
    if (!inFile)
    {
        cout << "[error] �ļ���ȡʧ�ܣ����ܶ�ȡ���ֽ�������: " << inFile.gcount() << " �ֽ�" << endl;
        delete[] filedata;
        inFile.close();
        return nullptr;
    }

    // �����������ĸ�����ʣ���ֽ�
    whole = size / UDP_DATAGRAM_SIZE;
    rest = size % UDP_DATAGRAM_SIZE;

    // �ر��ļ�
    inFile.close();
    cout << "[info] �ļ���ȡ�ɹ�����С: " << size << " �ֽ�" << endl;
    return filedata;
}


/* ������ */
bool headfile(char* fileName, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr, unsigned short& num_udp, unsigned short& rest, unsigned int& size)
{
    unsigned int t = strlen(fileName);
    // ����ļ���
    while (fileName[t] != '/' && t > 0)
        t--;
    // ƴ��total���ļ���
    char fileInfo[1024];  // ���ڴ洢 total + �ļ��� ��Ϣ
    if (rest % 2 != 0)
        rest += 1;
    // ���㱨������
    int total = (rest == 0 ? num_udp : num_udp + 1);
    cout << "�ܱ�����: " << total << endl;
    sprintf(fileInfo, "%d %s %d", total, fileName + t + 1, rest);  // ��ʽ��Ϊ "total:<total> <filename>"

    // ��ƴ�ӵ� total ���ļ�����Ϣ��rest��Ϣ���뱨��
    strcpy(send_DataGram.getmessage(), fileInfo);
    
    //����׼������
    memset(&send_DataGram, 0, sizeof(UDP_DATAGRAM));
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    send_DataGram.setseq(++nowseq);
    //seq=4
    send_DataGram.setstate(SYN);
    send_DataGram.setstate(REQ);
    // ������ĳ�ʼ����
    sendAndwait(send_DataGram, respond, sock, ServerAddr, ClientAddr);
}

//COPY//
/* �������ݱ� */
bool sendFileData(unsigned short& rest, unsigned short& num_udp, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr, UDP_DATAGRAM respond, char* fileContent)
{
    isTimerActive = 1;
    thread t1(timerLogic);
    for (int i = 0; i < num_udp; i++)
    {
        UDP_DATAGRAM data;
        memset(&data, 0, sizeof(UDP_DATAGRAM));
        // ȷ����������������ȷ���ƣ�����ʹ���ڴ�copy
        memcpy(data.getmessage(), fileContent + (i * UDP_DATAGRAM_SIZE), UDP_DATAGRAM_SIZE);
        data.setseq(++nowseq);
        data.setlength(unsigned short(UDP_DATAGRAM_SIZE) + 8);//����α�ײ�8�ֽ�
        if (sendAndwait(data, respond, sock, ServerAddr, ClientAddr))
        {
            cout << "*";
        }
        else
        {
            cout << "[error]����ʧ��!\n";
            t1.join();
            isTimerActive = 0;
            return 0;
        }
    }
    // �����������
    UDP_DATAGRAM last;
    memset(&last, 0, sizeof(UDP_DATAGRAM));
    last.setseq(++nowseq);
    // ���rest�Ƿ�Ϊ����
    if (rest % 2 != 0)
    {
        // rest ����������Ҫ����һ��ȫ���ֽ�
        memcpy(last.getmessage(), fileContent + (num_udp * UDP_DATAGRAM_SIZE), rest);
        last.getmessage()[rest] = 0;  // ���һ��ȫ���ֽ�
        last.setlength(rest + 8 + 1);  // ����length�ֶΣ������������ֽ�
    }
    else
    {
        // ���rest��ż����ֱ�Ӹ�������
        memcpy(last.getmessage(), fileContent + (num_udp * UDP_DATAGRAM_SIZE), rest);
        last.setlength(rest + 8);  // ����������length
    }

    if (sendAndwait(last, respond, sock, ServerAddr, ClientAddr)) {
        isTimerActive = 0;
        t1.join();
        return 1;
    }
    else
    {
        cout << "[error]����ʧ��!\n";
        isTimerActive = 0;
        t1.join();
        return 0;
    }

}



//COPY//
/* ���� */
int disConnect(SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    // ���ñ���
    memset(&send_DataGram, 0, sizeof(UDP_DATAGRAM));
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    //���ñ�־λFIN
    send_DataGram.setstate(FIN);
    send_DataGram.setseq(++nowseq);
    printf("%s", "��һ�λ���");
    // ���ͱ��Ĳ��ȴ�ȷ��
    if (sendAndwait(send_DataGram, respond, sock, ServerAddr, ClientAddr))
    {
        memset(&send_DataGram, 0, sizeof(UDP_DATAGRAM));
        printf("%s", "�����λ���");
        // ���ñ�־λACK
        send_DataGram.setstate(ACK);
        send_DataGram.setack(respond.getseq());
        send_DataGram.setseq(++nowseq);
        sendDatagram(send_DataGram, sock, ServerAddr);
        send_DataGram.showseq();
        return 1;
    }
    else
        return 0;
}

/// ��ȡ�ļ����ݲ��������ݺ�һЩԪ����
char* prepareFileContent(char* fileName, unsigned short& num_udp, unsigned short& rest, unsigned int& size) {
    return readFile(fileName, num_udp, rest, size);
}

// ��ʼ���ļ���������
bool initiateFileTransfer(char* fileName, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr, unsigned short num_udp, unsigned short rest, unsigned int size) {
    if (!headfile(fileName, sock, ServerAddr, ClientAddr, num_udp, rest, size)) {
        cout << "[error]�ļ���������ʧ��\n";
        return false;
    }
    return true;
}

// �����ļ�����
bool sendFileDataChunks(unsigned short rest, unsigned short num_udp, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr, char* fileContent) {
    if (!sendFileData(rest, num_udp, sock, ServerAddr, ClientAddr, respond, fileContent)) {
        cout << "[error]�ļ�����ʧ��\n";
        return false;
    }
    return true;
}

// �������ļ�����
int sendFile(char* fileName, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr) {
    unsigned short num_udp, rest;
    unsigned int size;

    // ��ȡ�ļ����ݲ���ȡ���Ԫ����
    char* fileContent = prepareFileContent(fileName, num_udp, rest, size);
    if (fileContent == NULL) {
        return 0; // ��ȡ�ļ�ʧ��
    }

    // ���� �ļ�ͷ ��������
    if (!initiateFileTransfer(fileName, sock, ServerAddr, ClientAddr, num_udp, rest, size)) {
        return 0; // ������ʧ��
    }

    // �����ļ���ֺõ�UDP����
    if (!sendFileDataChunks(rest, num_udp, sock, ServerAddr, ClientAddr, fileContent)) {
        return 0; // �����ļ�����ʧ��
    }

    return 1; // �ļ�����ɹ�
}

/* ���ӷ������к� */
void incrementSeqNumber()
{
    ++nowseq;
}

// ���Ͳ�ȷ�����ݱ�
bool sendAndReceiveAck(UDP_DATAGRAM& request, UDP_DATAGRAM& response, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    if (!sendAndwait(request, response, sock, ServerAddr, ClientAddr)) {
        return false;  // ����ʧ�ܻ�ȷ��ʧ��
    }
    return true;  // �ɹ����Ͳ��յ�ȷ��
}

// ��һ�����֣�����SYN���Ĳ��ȴ�ACK
bool firstHandshake(SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    UDP_DATAGRAM request, response;
    memset(&request, 0, sizeof(UDP_DATAGRAM));
    memset(&response, 0, sizeof(UDP_DATAGRAM));

    // ����SYN��־����������
    request.setstate(SYN);
    ++nowseq;  // �������к�
    request.setseq(nowseq);
    printf("%s", "��һ������:\n");

    // ����SYN���ȴ�ACK
    return sendAndReceiveAck(request, response, sock, ServerAddr, ClientAddr);
}

// �ڶ������֣����շ���������Ӧ������ACKȷ��
bool secondHandshake(SOCKET& sock, SOCKADDR_IN& ServerAddr, UDP_DATAGRAM& respond)
{
    UDP_DATAGRAM request;
    memset(&request, 0, sizeof(UDP_DATAGRAM));

    int t = respond.getseq();  // ��ȡ���������ص����к�
    request.setstate(ACK);      // ����ACK��־
    request.setack(t);          // ����ACKΪ��������seq
    ++nowseq;                   // �������к�
    request.setseq(nowseq);     // ���ñ���seq
    printf("%s", "����������:\n");

    // ����ACKȷ�ϱ���
    sendDatagram(request, sock, ServerAddr);
    request.showseq();  // ��ʾ��ǰ���к�

    return true;
}

// �������ӣ�ִ���������ֹ���
bool initateConnection(SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    UDP_DATAGRAM respond;

    // ��һ������
    if (!firstHandshake(sock, ServerAddr, ClientAddr)) {
        return false;  // ��һ������ʧ��
    }

    // �ڶ������֣����շ�������Ӧ������ȷ��
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    int len = sizeof(SOCKADDR);
    recvfrom(sock, (char*)&respond, sizeof(UDP_DATAGRAM), 0, (struct sockaddr*)&ServerAddr, &len);

    return secondHandshake(sock, ServerAddr, respond);
}


int main()
{
    WSADATA WSAData;
    if (0 != WSAStartup(MAKEWORD(1, 1), &WSAData))
    {
        printf("%s", "[error]WSAStartup����!\n");
        return 0;
    }
    //�뱾��IP��
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    unsigned long l = 1;
    ioctlsocket(sock, FIONREAD, &l);
    //������ģʽ

    SOCKADDR_IN ClientAddr;
    ClientAddr.sin_addr.s_addr = inet_addr(&IPClient[0]);
    ClientAddr.sin_family = AF_INET;
    ClientAddr.sin_port = htons(CLIENTPORT);
    bind(sock, (SOCKADDR*)&ClientAddr, sizeof(SOCKADDR));

    SOCKADDR_IN ServerAddr;
    ServerAddr.sin_addr.s_addr = inet_addr(&IPServer[0]);
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(SERVERPORT);

    char file_path[200];
    QueryPerformanceFrequency(&frequency);

    // ��������
    if (initateConnection(sock, ServerAddr, ClientAddr))
    {
        string a = "���뵥��ĸT�����ļ���\n���뵥��ĸQ�˳�\n";
        cout << "�����������ӳɹ�";
        while (1)
        {
            char m;
            cout << a;
            cin >> m;

            if (m == 'q')
            {
                // �Ͽ�����
                disConnect(sock, ServerAddr, ClientAddr);
                break;
            }
            else {
                //ѡ�����ļ�
                //cin.getline(file_path,200);

                strcpy(file_path, "D:/28301/Desktop/new/helloworld.txt");
                int ans = 0;
                ans = sendFile(file_path, sock, ServerAddr, ClientAddr);
                if (ans)
                {
                    cout << "�ļ�����ɹ�";
                }
            }

            nowseq = 1; // ����
        }
    }
    else
        cout << "[error]\n";
    closesocket(sock);
    WSACleanup();
    return 0;
}



