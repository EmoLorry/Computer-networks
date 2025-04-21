#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <iostream>
#include <thread>
#include <string>
#include <time.h>
#include <fstream>
#include <sstream>
#include <ws2tcpip.h>
#include <vector>
#pragma comment(lib, "ws2_32.lib")

//�궨��˿��Լ�IP
#define SERVER_ip "127.0.0.1"
#define CLIENT_ip "127.0.0.1"
#define SERVER_port 10000       // �������˿�
#define CLIENT_port 40000     // �ͻ��˶˿�
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
#define timelimit_udp 150 //�����ڵı���0.15sδȷ�����ش�
//ȫ�ֱ�����������������ֹλ�á��������кš�udp���ݱ������Լ����һ�����ݱ����ֽ���
int windowtop = 0;
int windowbase = 0;
int sendseq = 0;
pack udp_num;
pack rest;

// �����̳߳أ����ڹ������з�Ƭ�����߳�
std::vector<std::thread> threads;
//��ʼ�������������֧�ַ�Χ�ڿ����õ��Ļ���ռ�
char buffer[65536][UDP_DATAGRAM_SIZE];
//��ʼ�����ݱ������ȷ������
bool acked[65536];
bool isthread[65536];
int accumulate_point = -1;//�ۻ�ȷ��ָ��
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

void print_red_message(int seq) {
    // ANSI ת�����У���ɫ����
    const string RED = "\033[31m";
    // ANSI ת�����У�������ɫ
    const string RESET = "\033[0m";

    // ��ӡ����ɫ�����
    cout << RED << "����Ϊ" << seq << "�ı��ĳ�ʱδȷ�ϣ������ش�����" << RESET << endl;
}

void print_yellow_message(int start,int end) {
    // ANSI ת�����У���ɫ����
    const string RED = "\033[33m";
    // ANSI ת�����У�������ɫ
    const string RESET = "\033[0m";
    cout << RED << "========================������������1λ=========================" << endl;
    // ��ӡ����ɫ�����
    cout << "             ���ڵײ�����: "<< start << "�����ڶ�������:"<<end<<endl;
    cout << "================================================================" << RESET<< endl;
    cout << endl;
}

//�ͻ��ˣ����������Ͷ��������������������ֺ͡����λ��֡�ԭ��
int connection() {
    UDP_DATAGRAM build_first, build_second, build_third;

    //===================================================================================================
    //��һ������
    sendseq += 1;
    build_first.setstate(SYN);
    build_first.setseq(sendseq);
    build_first.calculate_checksum(); // ���㲢����У���

    if (sendto(sock, (char*)&build_first, sizeof(build_first), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
        cout << "�������ķ��ʹ��󣬴�����: " << WSAGetLastError() << endl;
        return 0;
    }
    else {
        cout << "��һ�����ֽ������ķ��ͳɹ�" << endl;
    }
    //cout << "��" << "senseq" << "���ķ��ʹ���" << endl;
    int start = clock();//��¼ʱ��


    //===================================================================================================
    int relimit = 0;//�������10���ط�
    // ȷ�ϵڶ�������
    int end; // ��ǰʱ�����
    cout << "�����У�******";
    while (true) {
        end = clock(); // ��ȡ��ǰʱ��
        if (end - start > timelimit)
        {
            if (relimit == 10)
                return 0;
            start = clock();//���¼�ʱ
            cout << "������ʱ���ط���һ������" << endl;
            relimit++;
            if (sendto(sock, (char*)&build_first, sizeof(build_first), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
                cout << "�������ķ��ʹ��󣬴�����: " << WSAGetLastError() << endl;
                return 0;
            }

        }

        recvfrom(sock, (char*)&build_second, sizeof(build_second), 0, (SOCKADDR*)&server, &l);
        if ((build_second.getack() == sendseq + 1) && build_second.getstate() & SYN && build_second.getstate() & ACK)
        {
            relimit = 0;
            break;
        }
        cout << "***";
    }

    //===================================================================================================
    // ����������
    sendseq += 1;
    build_third.setstate(ACK);
    build_third.setseq(sendseq);
    build_third.calculate_checksum(); // ���㲢����У���
    if (sendto(sock, (char*)&build_third, sizeof(build_third), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
        cout << "�������ķ��ʹ��󣬴�����: " << WSAGetLastError() << endl;
        return 0;
    }
    else
        return 1;

}

//����һ�����ݱ��ĺ��ĺ���������ʽ������&���������ȷ�ϣ����ջᴥ����ʱ�ش���
// ע��ÿһ�������ڵ����ݱ����ܻ���Ӱ�죬ÿһ�����ݱ��Ĵ���Ӧ����һ���߳�
int udpdatagramcopyandsend(int seq) {
    UDP_DATAGRAM udp_now;
    memset(&udp_now, 0, sizeof(UDP_DATAGRAM));
    if (seq != udp_num - 1)
    {
        memcpy(udp_now.getmessage(), buffer[seq], UDP_DATAGRAM_SIZE);
        udp_now.setlength(UDP_DATAGRAM_SIZE);
        udp_now.setseq(seq);
    }
    else
    {
        memcpy(udp_now.getmessage(), buffer[seq], rest);
        udp_now.setlength(rest);
        udp_now.setseq(seq);
    }
    udp_now.setstate(SYN);
    udp_now.calculate_checksum(); // ���㲢����У���
    //=========================================================================================
    //�������ݱ�
    if (sendto(sock, (char*)&udp_now, sizeof(udp_now), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
        cout << "����Ϊ" << seq << "�����ݱ��ķ��ʹ��󣬴�����: " << WSAGetLastError() << endl;
        return 0;
    }
    //cout << "����Ϊ" << seq << "�����ݱ��ķ��ͳɹ���"  << endl;

// ע����ı���Ҳ��δȷ�ϣ�������ʱ����ش�����
    int start = clock(); // ��¼������ʼʱ��

    while (true)
    {
        int end = clock(); // ��ȡ��ǰʱ��

        // �ȴ�����ȷ�ϣ���ʱ�ش�
        if (end - start > timelimit_udp)
        {
            print_red_message(seq);
            //cout << "����Ϊ" <<  << "�ı��ĳ�ʱδȷ�ϣ������ش�����" << endl;
            //�������ݱ�
            if (sendto(sock, (char*)&udp_now, sizeof(udp_now), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
                //cout << "����Ϊ" << seq << "�����ݱ��ķ��ʹ��󣬴�����: " << WSAGetLastError() << endl;
                return 0;
            }
            else
                //cout << "����Ϊ" << seq << "�����ݱ��ķ��ͳɹ���" << endl;
                start = clock();  // ������ʼʱ��
        }

        //�߳��˳�������
        if (acked[seq])
            return 1; // ���� 1 ��ʾ��Ϣ���ͳɹ�����ȷ��  

        //Sleep(50);
    }

}

//���Ͷ˺��Ĵ�����������ʽ���������ڣ��������Ϊ�˺ͻ����������ú������У�����Ϊ�߳�
int sending() {
    // ���ϱ�����⣬���ͻ��������ڵ�δȷ�Ϸ�Ƭ
    while (true) {
        // ��⵽����������Ϻ�ֱ���˳�
        if (windowbase == udp_num - 1 && acked[windowbase])
            break;

        for (int i = windowbase; i <= windowtop; i++) {
            if (!acked[i] && !isthread[i]) { // ����Ƭ״̬��ֻ���ʹ����ڱ�ʾδȷ�ϵı���
                // ����һ���̷߳��ͷ�Ƭ,���������߳�û��Ҫ�ٽ���
                threads.emplace_back(std::thread(udpdatagramcopyandsend, i));
                isthread[i] = true;
            }
        }
    }

    // �ȴ����з����߳����
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join(); // �����ȴ��߳̽���
        }
    }

    //�����ļ�����ȷ�ϱ���
    int timeold = clock();
    UDP_DATAGRAM udp_end;
    memset(&udp_end, 0, sizeof(UDP_DATAGRAM));
    udp_end.setstate(SYN);
    udp_end.setstate(FIN);

    if (sendto(sock, (char*)&udp_end, sizeof(udp_end), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR)
        return 0;


    while(true){
        int timenew = clock();
        if (timenew - timeold > timelimit)
        {
            //��ʱ�ش�
            int timeold = clock();//���¼�ʱ
            UDP_DATAGRAM udp_end;
            memset(&udp_end, 0, sizeof(UDP_DATAGRAM));
            udp_end.setstate(SYN);
            udp_end.setstate(FIN);

            if (sendto(sock, (char*)&udp_end, sizeof(udp_end), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR)
                return 0;

        }
        
    UDP_DATAGRAM udp_end_recv;
    memset(&udp_end_recv, 0, sizeof(UDP_DATAGRAM));
    recvfrom(sock, (char*)&udp_end_recv, sizeof(UDP_DATAGRAM), 0, (SOCKADDR*)&server, &l);
    if (udp_end_recv.getstate() & ACK && udp_end_recv.getstate() & FIN)
    {
        cout << "��ϲ���������ݱ��ľ�����ȷ���գ�";
        break;
    }
    }

    return 1; // ���ͺ������
}



    int receive() {
        while (true)
        {
            UDP_DATAGRAM recv;
            memset(&recv, 0, sizeof(UDP_DATAGRAM));
            recvfrom(sock, (char*)&recv, sizeof(UDP_DATAGRAM), 0, (SOCKADDR*)&server, &l);
            if (recv.getstate() & REQ)
            {
                cout << "��������Ϊ" << recv.getack() << "�ı��Ĵ���������ݴ���" << endl;
                continue;
            }

            else if (recv.getstate() & ACK)
            {
                cout << "�ۻ�ȷ�ϣ����к�" << recv.getack() << "֮ǰ���������ݱ���ȷ�ϣ�" << endl;
                for(int j= accumulate_point+1;j<= recv.getack();j++)
                    acked[j] = 1;          // ����״̬����ʾ�ۻ�ȷ��
                accumulate_point = recv.getack();//�����ۻ�ȷ��ָ��
                cout << "===============================================================" << endl;



            }
            //�߳��˳�����
            //���������������ڵ�����Ҽ�鴰���ڵ��Ƿ���ȷ��
            bool exitjudge = true;
            for (int i = windowbase; i <= udp_num - 1; i++)
                if (!acked[i])
                    exitjudge = false;

            if (exitjudge)
                return 1; // ���ȷ�Ϻ������һ����Ƭ���˳������߳�
        }
    }

    int disconnection() {
        UDP_DATAGRAM bye_first, bye_second, bye_third;
        return 1;
    }




    // ��ȡ�ļ����ݲ�������Ӧ�������ص�������
    void dealwithfiledata(char* fileName) {
        // ���ļ�
        ifstream inFile(fileName, ios::in | ios::binary);
        if (!inFile.is_open()) {
            cout << "[error] �ļ���ʧ��: " << fileName << endl;
            return;
        }

        // ��ȡ�ļ���С
        inFile.seekg(0, ios::end);
        unsigned int size = static_cast<unsigned int>(inFile.tellg());
        if (size == 0) {
            cout << "[error] �ļ�Ϊ��: " << fileName << endl;
            inFile.close();
            return;
        }

        // �����ļ���ͷ
        inFile.seekg(0, ios::beg);

        // �����������ĸ�����ʣ���ֽ�
        udp_num = size / UDP_DATAGRAM_SIZE;
        rest = size % UDP_DATAGRAM_SIZE;

        //���ļ�������Ϣ�ŵ��ױ�����
        char fileInfo[1024];  // ���ڴ洢 total + �ļ��� ��Ϣ
        unsigned int t = strlen(fileName);
        // ����ļ���
        while (fileName[t] != '/' && t > 0)
            t--;


        // ��ȡ�ļ����ݵ�������
        for (unsigned short i = 1; i < udp_num + 1; ++i) {
            inFile.read(buffer[i], UDP_DATAGRAM_SIZE);
            if (!inFile) {
                cout << "[error] �ļ���ȡʧ�ܣ���ȡ���� " << i << " �����ݱ�ʱ����" << endl;
                inFile.close();
                return;
            }
        }

        // ��ȡʣ���ֽڵ����һ�����ݱ�
        if (rest > 0) {
            udp_num += 1;//ע��udp_num�ĺ���
            inFile.read(buffer[udp_num + 1], rest);
            if (!inFile) {
                cout << "[error] �ļ���ȡʧ�ܣ���ȡʣ���ֽ�ʱ����" << endl;
                inFile.close();
                return;
            }
        }

        // ���ʣ���ֽ���Ϊ���������һ��ȫ���ֽ�
        if (rest % 2 != 0) {
            buffer[udp_num + 1][rest] = '\0';  // ��ʣ�����ݵĺ������һ�����ֽ�
            ++rest;  // ����ʣ���ֽ���
        }

        //������rest����ļ�������Ϣ�ŵ��ױ�����
        // ƴ��total���ļ���
        sprintf(fileInfo, "%d %s %d", udp_num + 1, fileName + t + 1, rest);  // ��ʽ��Ϊ "<total> <filename><rest>"
        strcpy(buffer[0], fileInfo);

        //����ͷ����
        udp_num += 1;

        // �ر��ļ�
        inFile.close();
        cout << "[info] �ļ���ȡ�ɹ�����С: " << size << " �ֽڣ��������ݱ�����: " << udp_num
            << "��β���ݱ��ֽ���: " << rest << endl;
    }


    //�����������������ڣ�һֱѭ�����������ݱ��ķ����ҽ���ȷ�����
    void windowsending() {
        while (true) {
            if (acked[windowbase])
            {
                if (windowbase == udp_num - 1)
                    break;//��������ȫ��ȷ�����
                windowbase++;     // �������ڵײ�����һλ    
                if (windowtop < udp_num - 1)
                    windowtop++;
                print_yellow_message(windowbase, windowtop);
            }

        }
        return;
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
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        u_long mode = 0;  // 1 ��ʾ������ģʽ
        if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
            cout << "[error] �޷���������ģʽ, ������: " << WSAGetLastError() << endl;
            closesocket(sock);
            WSACleanup();
            return -1;
        }

        bind(sock, (SOCKADDR*)&client, sizeof(SOCKADDR));
        if (sock == INVALID_SOCKET) {
            cout << "�׽��ִ���ʧ��" << endl;
            return 0;
        }

        server.sin_addr.S_un.S_addr = inet_addr(SERVER_ip);
        server.sin_family = AF_INET;
        server.sin_port = htons(SERVER_port);

        if (connection())
            cout << "�����ɹ�!" << endl;
        else
        {
            cout << "����ʧ�ܣ�" << endl;
            return 0;
        }




        cout << "������Ҫ������ļ���·��" << endl;
        string path;
        cin >> path;

        //D:/28301/Desktop/new/helloworld.txt
        //D:/28301/Desktop/new/2.jpg
        // D:/28301/Desktop/new/1.jpg
        //D:/28301/Desktop/new/3.jpg
        //�����ļ������ļ����ݼ��ص���������������UDP���ݱ�����
        dealwithfiledata(&path[0]);

        //ע��windowtop��windowbase���Ǵ��ڵ�λ��ָ��
        //��ʼ��windowtop��windowbase
        windowbase = 0;
        if (udp_num - 1 <= windowlength)
            windowtop = udp_num - 1;
        else
            windowtop = windowlength;

        //��ʼ��acked[udp_num]����
        for (int i = 0; i < udp_num; i++)
            acked[i] = false;
        for (int i = 0; i < udp_num; i++)
            isthread[i] = false;

        int timestart = clock(); // ��ʼ��ʱ

        // ���������̣߳�������շ�����ȷ����Ϣ
        thread recv(receive);
        recv.detach(); // ���̷߳���

        // ���������̣߳��������ļ���Ƭ
        cout << "��ʼ�����ļ���Ӧ��UDP���ݱ�����" << endl;
        thread send(sending);
        send.detach(); // ���̷߳���


        //���ڻ���ȷ�ϴ�����������,ֱ�����б��Ķ�����ȷ����ȷ��
        windowsending();

        int timeend = clock(); // ������ʱ

        // ���㲢�����������
        double endtime = (double)(timeend - timestart) / CLOCKS_PER_SEC;
        cout << "�ļ�����ʱ��" << endtime << "s" << endl;
        cout << "����������" << (double)(udp_num) * (sizeof(UDP_DATAGRAM) << 3) / endtime / 1024 << "kb/s" << endl;


        //���λ���
        if (disconnection())
            cout << "�����ɹ�" << endl;

        closesocket(sock);
        WSACleanup();
        system("pause");
        return 0;
    }