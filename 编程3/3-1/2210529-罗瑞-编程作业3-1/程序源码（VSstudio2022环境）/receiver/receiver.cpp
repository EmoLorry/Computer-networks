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

#define RESENT_TIMES_LIMIT 3                 // 重传次数
#define TIME_LIMIT 10                         // 超时
#define SERVER_FILE_PATH "D:/28301/Desktop/get/" // server文件保存位置

int status;//描述是否建立了有效连接
LARGE_INTEGER frequency; //计时器频率


//一次最多支持传输0.5GB大小的文件
#define UDP_DATAGRAM_SIZE (8 * 1024) //UDP报文数据大小设置(unsigned short类型变量即可控制)

//设置通信的IP和端口号
std::string IPServer = "127.0.0.1";
std::string IPClient = "127.0.0.1";
u_short SERVERPORT = 20000;
u_short CLIENTPORT = 40000;


//四个状态位的定义,让不同的状态组合都可以用四位标志变量表示，
// 实际上用了2字节大小的unsigned short类型变量state来记录
#define SYN 0x01
#define ACK 0x02
#define FIN 0x04
#define REQ 0x08

//UDP伪首部(4+4+ 1+1(合并) +2) 类
struct pseudoheader {
public:
    unsigned int source_ip;
    unsigned int destination_ip;
    unsigned short con;
    unsigned short length;
};



//UDP数据报类
class UDP_DATAGRAM {

    //UDP首部
    unsigned short source_port;
    unsigned short destination_port;
    unsigned short length;
    //校验和：大小2字节，需要4个16进制位
    unsigned short checksum;

    //UDP数据（为了保证校验和的正常计算，必须为偶数个字节，若传入奇数字节，自动在某位加一个全零字节）
    char message[UDP_DATAGRAM_SIZE];

    //保证可靠传输协议实现的变量
    unsigned short state;//状态
    unsigned short seq, ack;//序列号和确认号


public:
    void showseq() {std::cout << "\n传输序列seq为" << this->seq << endl; }
    //设置ack
    void setack(unsigned short ack) { this->ack = ack; }
    //获取ack
    unsigned short getack() { return this->ack; }
    //设置seq
    void setseq(unsigned short seq) { this->seq = seq; }
    //获取seq
    unsigned short getseq() { return this->seq; }
    //设置state
    void setstate(unsigned short state) { this->state = state | this->state; }
    //获取state
    unsigned short getstate() { return this->state; }
    //获取数据地址
    char* getmessage() { return this->message; }
    //设置UDP长度
    void setlength(unsigned short length) { this->length = length; }
    //获取UDP长度
    unsigned short getlength() { return this->length; }
    //设置UDP校验位
    void calculate_checksum() {
        //进行溢出操作，因此sum变量要大于两字节
        unsigned int sum = 0;
        //在此函数中完成port字段与伪首部字段初始化
        this->source_port = CLIENTPORT;
        this->destination_port = SERVERPORT;
        struct pseudoheader* udp_header = new pseudoheader();
        udp_header->source_ip = stringtoint(IPClient);
        udp_header->destination_ip = stringtoint(IPServer);
        udp_header->con = 17;
        udp_header->length = this->getlength();
        //求和(两字节对齐相加)

        // 逐字段累加伪首部(两字节对齐相加)
        sum += (udp_header->source_ip >> 16) & 0xFFFF;
        sum += udp_header->source_ip & 0xFFFF;

        sum += (udp_header->destination_ip >> 16) & 0xFFFF;
        sum += udp_header->destination_ip & 0xFFFF;

        sum += udp_header->con;
        sum += udp_header->length;

        //逐字段累加 UDP 报文首部以及数据(两字节对齐相加)
        unsigned short* point = (unsigned short*)this;
        int udp_header_length = sizeof(*this) / 2;
        for (int i = 0; i < udp_header_length; i++) {
            sum += *point++;
        }

        // 溢出处理
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        // 取反
        this->checksum = ~((unsigned short)sum);

    };
    //校验UDP校验位
    bool check_checksum() {
        //进行溢出操作，因此sum变量要大于两字节
        unsigned int sum = 0;
        //在此函数中依然要设置伪首部字段
        struct pseudoheader* udp_header = new pseudoheader();
        udp_header->source_ip = stringtoint(IPClient);
        udp_header->destination_ip = stringtoint(IPServer);
        udp_header->con = 17;
        udp_header->length = this->getlength();
        //求和(两字节对齐相加)

        // 逐字段累加伪首部(两字节对齐相加)
        sum += (udp_header->source_ip >> 16) & 0xFFFF;
        sum += udp_header->source_ip & 0xFFFF;

        sum += (udp_header->destination_ip >> 16) & 0xFFFF;
        sum += udp_header->destination_ip & 0xFFFF;

        sum += udp_header->con;
        sum += udp_header->length;

        //逐字段累加 UDP 报文首部以及数据(两字节对齐相加)
        unsigned short* point = (unsigned short*)this;
        int udp_header_length = sizeof(*this) / 2;
        for (int i = 0; i < udp_header_length; i++) {
            sum += *point++;
        }

        // 溢出处理
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        // 检查是否全1
        if (sum == 0xFFFF)
        {
            printf("%s", "校验正确\n");
            return true;
        }
        printf("%s", "校验出错\n");
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
    //覆盖原理设置差错
    if (lost % 5 == 0)
        memset(&datagram, 0, sizeof(UDP_DATAGRAM));
   
    return res;
}


/* 停等协议SEND */
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


/* 停等协议 RECV */
bool RCACK(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& ClientAddr) {
    UDP_DATAGRAM ack;
    memset(&ack, 0, sizeof(UDP_DATAGRAM));

    while (true) {
        // 接收数据报文
        recvDatagram(datagram, sock, ClientAddr);
        std::cout << "Received Seq: " << datagram.getseq() << " | Expected Seq: " << sendseq << std::endl;

        // 校验和检测
        if (!datagram.check_checksum()) {
            std::cout << "[Error] 校验和错误\n";

            // 构造请求重传的ACK报文
            ack.setstate(REQ);
            ack.setseq(datagram.getseq());
            sendDatagram(ack, sock, ClientAddr);

            std::cout << "发送请求重传的ACK报文，序号: ";
            ack.showseq();
            std::cout << "数据报长度: " << datagram.getlength() << std::endl;

            // 清空报文数据
            memset(&ack, 0, sizeof(UDP_DATAGRAM));
            memset(&datagram, 0, sizeof(UDP_DATAGRAM));
            continue; // 继续等待接收
        }

        // 检查序列号是否匹配
        if (datagram.getseq() == sendseq) {
            std::cout << "[Info] 序列号匹配\n";

            // 构造确认ACK报文
            ack.setack(datagram.getseq());
            ack.setstate(ACK);
            ack.setseq(sendseq);
            sendDatagram(ack, sock, ClientAddr);

            // 更新发送序列号并返回成功
            sendseq++;
            std::cout << "发送ACK确认报文，序号: ";
            ack.showseq();
            return true;
        }

        // 序列号不同步的情况
        std::cout << "[Error] 序列号不同步，当前报文序号: " << datagram.getseq()
            << "，期望序号: " << sendseq << std::endl;
        break; // 退出循环
    }

    return false; // 接收失败
}

/* 接收文件 */
int recvFile(UDP_DATAGRAM datagram, SOCKET& sock, SOCKADDR_IN& ClientAddr, SOCKADDR_IN& ServerAddr)
{
    // 用于存放文件路径
    char fileName[100];
    // 接收初始的文件传输请求报文（包含文件名信息和UDP个数信息）
    // 解析接收到的报文，获取 total 和文件名
    char fileInfo[1024];
    memset(fileInfo, 0, sizeof(fileInfo));
    strcpy(fileInfo, datagram.getmessage());

    // 解析 total 和 fileName 和rest
    int total = 0;
    int rest = 0;
    char extractedFileName[100];
    sscanf(fileInfo, "%d %s %d", &total, extractedFileName,&rest);


    // 拼接完整的文件路径
    strcpy(fileName, SERVER_FILE_PATH);
    strcat(fileName, extractedFileName);

    
    UDP_DATAGRAM back;
    memset(&back, 0, sizeof(UDP_DATAGRAM));
    back.setack(datagram.getseq());
    back.setseq(++sendseq);
    //seq=5
    sendDatagram(back, sock, ClientAddr);
    cout << "已确认文件请求报文" << endl;
    LARGE_INTEGER clockstart, clockend;
    QueryPerformanceCounter(&clockstart);

    unsigned int whole=0;
    // 计算完整报文个数及非整报文大小
    if (rest == 0)
        whole = total;
    else
        whole = total - 1;

    int size = UDP_DATAGRAM_SIZE * whole + rest;
    cout <<"报文总字节大小为" << size<<endl;
    //初始化要存放数据的字符串
    char* recvbuf = (char*)malloc(size);

    for (int i = 0; i < whole; i++)
    {
        cout << "接收报文文件列" <<i<< endl;
        UDP_DATAGRAM data;
        memset(&data, 0, sizeof(UDP_DATAGRAM));
        RCACK(data, sock, ClientAddr);
        memcpy(recvbuf + i * UDP_DATAGRAM_SIZE, data.getmessage(), UDP_DATAGRAM_SIZE);
    }

    //接收剩余的数据
    UDP_DATAGRAM data, respond;
    memset(&data, 0, sizeof(UDP_DATAGRAM));
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    cout << "rest 大小为 " << rest << endl;
    RCACK(data, sock, ClientAddr);
    memcpy(recvbuf + (whole * UDP_DATAGRAM_SIZE), data.getmessage(), rest);

    //写入文件
    FILE* file;
    file = fopen(fileName, "wb");
    if (recvbuf != 0)
    {
        fwrite(recvbuf, size, 1, file);
        fclose(file);
    }
    QueryPerformanceCounter(&clockend);
    double duration = (double)(clockend.QuadPart - clockstart.QuadPart) / (double)frequency.QuadPart;
    // 记录传输时间和吞吐量
    cout << "\n传输时间:" << duration << "秒\n";
    cout << "吞吐率:" << size / duration << "char/s\n----------------------------------\n";
    return 1;
}

/* 建连 */
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
    
    // 发送报文确认连接请求
    sendDatagram(respond, sock, addr);
    respond.showseq();
    int len = sizeof(SOCKADDR);
    //接收第三次握手
    recvfrom(sock, (char*)&client_last_respond, sizeof(UDP_DATAGRAM), 0, (struct sockaddr*)&addr, &len);
    if (client_last_respond.getstate() & (ACK))
        return 1;
    else
        return 0;
}

/* 断连 */
int disConnect(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    // 初始化报文
    UDP_DATAGRAM data;
    UDP_DATAGRAM respond;
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    memset(&data, 0, sizeof(UDP_DATAGRAM));
    // 设置标志位FIN与ACK
    data.setstate(FIN);
    data.setstate(ACK);
    data.setack(datagram.getseq());
    // 发送报文并等待确认
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
        cout << "WSA启动失败\n";
        return 0;
    }
    //与本地IP绑定
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval timeout;

    SOCKADDR_IN ServerAddr;
    ServerAddr.sin_addr.s_addr = inet_addr(&IPServer[0]);
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(SERVERPORT);
    bind(sock, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));

    //接收端的IP地址
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
                cout << "建立连接\n";
            }
            else
                cout << "连接建立失败\n";
        }
        else if (data.getstate()&(FIN))
        {
            if (disConnect(data, sock, ServerAddr, ClientAddr))
            {
                status = 0;
                cout << "断连成功\n";
                //重置
                sendseq = 1;
            }
        }
        else if ((data.getstate() & (SYN)) && (data.getstate() & (REQ)))
        {
            if (status)
            {
                recvFile(data, sock, ClientAddr, ServerAddr);
                //重置
                sendseq = 1;
            }
        }
    }
    closesocket(sock);
    WSACleanup();
    return 0;
}