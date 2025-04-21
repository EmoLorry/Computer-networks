#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SERVER_FILE_PATH "D:/28301/Desktop/get/" // server文件保存位置
// 全局变量
// 用于拼接完整的文件路径
char FileName[100];

//====================================================================================
//作者 ：罗瑞
//2022级
//====================================================================================



#include <cstdlib> // 包含 rand() 和 srand()
#include <ctime>   // 包含 time()
#include <WinSock2.h>
#include <iostream>
#include <thread>
#include <string>
#include <time.h>
#include <fstream>
#include <sstream>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

//宏定义端口以及IP
#define SERVER_ip "127.0.0.1"
#define CLIENT_ip "127.0.0.1"
#define SERVER_port 10001      // 服务器端口
#define CLIENT_port 40001      // 客户端端口
SOCKADDR_IN server, client; //全局定义服务端和客户端的SOCKADDR_IN，避免麻烦的引用传参
SOCKET sock; // 套接字
int l = sizeof(SOCKADDR);

//一次最多支持传输0.5GB大小的文件
#define UDP_DATAGRAM_SIZE (8 * 1024) //UDP报文数据大小设置(unsigned short类型变量即可控制)
#define pack unsigned short  //最多支持65536个数据包

//宏定义滑动窗口大小、超时参数
//大小是26个UDP数据报
#define windowlength 25
#define timelimit 100 //100ms
//全局变量滑动窗的序列起止位置、发送序列号、udp数据报个数以及最后一个数据报的字节数
int windowtop = 0;
int windowbase = 0;
int sendseq = 0;
pack udp_num;
pack rest;




//文件接收缓冲区，最大支持范围内可能用到的缓冲空间
char buffer[65536][UDP_DATAGRAM_SIZE];
//初始化数据报组接收确认数组,

//累积确认记录数组
bool acked[65536] = { false };
//累积确认指针
int accumulate_point = -1;

using namespace std;

//函数：IP格式转化为整形
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

//UDP伪首部类(4+4+ 1+1(合并) +2)
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
    void showseq() { cout << "\n传输序列seq为" << this->seq << endl; }
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
        this->source_port = CLIENT_port;
        this->destination_port = SERVER_port;
        struct pseudoheader* udp_header = new pseudoheader();
        udp_header->source_ip = stringtoint(CLIENT_ip);
        udp_header->destination_ip = stringtoint(SERVER_ip);
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
        udp_header->source_ip = stringtoint(CLIENT_ip);
        udp_header->destination_ip = stringtoint(SERVER_ip);
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
            cout << "校验和为" << this->checksum << endl;
            printf("%s", "UDP校验正确");
            return true;
        }
        return false;
    }
};

//四种状态宏定义，采用类型为unsigned short 类型的状态变量state
// 采用类似于三次握手的建连机制
// SYN 建连请求标志 
// SYN & ACK 建连回复确认
// ACK 建连确认

// 此场景下只需要三次挥手即可确保断连功能
// FIN 断连请求标志
// FIN &ACK 断连回复确认
// ACK 断连 

// SYN & REQ 头文件标志(发送文件的相关信息)
// ACK发送文件数据报确认标志
#define SYN 1
#define ACK 2
#define FIN 4
#define REQ 8

static int countcut = 0;


//服务器：建连函数和断连函数，采用三次握手和“三次挥手”原则
int connection() {
    UDP_DATAGRAM build_first, build_second, build_third;
    //======================================================================================
    // 收到第一次握手
    cout << "建连中：******";
    while (true) {
        recvfrom(sock, (char*)&build_first, sizeof(build_first), 0, (SOCKADDR*)&client, &l);
        if (build_first.getstate() & SYN)
            break;
    }

    //======================================================================================
    //发送第二次握手
    sendseq++;
    build_second.setstate(SYN);
    build_second.setstate(ACK);
    build_second.setseq(sendseq);
    build_second.setack(build_first.getseq() + 1);
    if (sendto(sock, (char*)&build_second, sizeof(build_second), 0, (SOCKADDR*)&client, l) == SOCKET_ERROR) {
        cout << "第二次握手建连报文发送错误，错误码: " << WSAGetLastError() << endl;
        return 0;
    }
    else {
        cout << "第二次握手建连报文发送成功";
    }
    //======================================================================================
    // 收到第三次握手
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
           //真：随机丢包
            //设置随机种子为当前时间
        countcut++;
        // 生成范围为 1 到 10000 的随机数
        int a = countcut % 100;//随机丢包率，1％
        if (!a)
            continue;//跳过后面操作，来模拟丢包
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
            cout << "收到序号为" << recv.getseq() << "的报文," << endl;
//============================================================================================================
//      //快重传的方法基础 ———Reno算法，对新确认的报文进行回复，失序的情况就会重复地回复最后一个确认的ack
            //回复确认
            UDP_DATAGRAM sen;
            memset(&sen, 0, sizeof(UDP_DATAGRAM));
            sen.setack(accumulate_point_now);
            sen.setstate(ACK);
            sendto(sock, (char*)&sen, sizeof(sen), 0, (SOCKADDR*)&client, l);
            if (accumulate_point == accumulate_point_now)
            {
                cout << endl;
                cout << "报文接收失序！对序列号" << accumulate_point_now << "重复确认！" << endl;
                cout << "==============================================================" << endl;
            }
            else
            {
                cout << endl;
                cout << "回复序列号为" << accumulate_point_now << "的累积确认！" << endl;
                cout << "==============================================================" << endl;
            }
            
            
            accumulate_point = accumulate_point_now;


                //============================================================================================
            if (recv.getseq() == 0)
            {

                // 接收初始的文件传输请求报文（包含文件名信息和UDP个数信息）
                // 解析接收到的报文，获取 udp_num 和文件名
                // 用于存放文件名
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
            //接收到文件发送确认报文
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
        printf("%s", "[error]WSAStartup错误!\n");
        return 0;
    }

    client.sin_addr.S_un.S_addr = inet_addr(CLIENT_ip);
    client.sin_family = AF_INET;
    client.sin_port = htons(CLIENT_port);// 设置服务器端口号


    server.sin_addr.S_un.S_addr = inet_addr(SERVER_ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_port);
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    u_long mode = 0;  // 1 表示非阻塞模式
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        cout << "[error] 无法设置阻塞模式, 错误码: " << WSAGetLastError() << endl;
        closesocket(sock);
        WSACleanup();
        return -1;
    }
    bind(sock, (SOCKADDR*)&server, sizeof(SOCKADDR));
    if (sock == INVALID_SOCKET) {
        cout << "套接字创建失败" << endl;
        return 0;
    }

    if (connection())
        cout << "建连成功!" << endl;
    else
        cout << "建连失败！" << endl;

    if (receiveandack())
        cout << "文件接收成功！" << endl;

    //三次挥手
    if (disconnection())
        cout << "断连成功" << endl;

    if (writedata())
        cout << "写文件完成！" << endl;
    else
        cout << "写文件失败！" << endl;




    return 0;
}