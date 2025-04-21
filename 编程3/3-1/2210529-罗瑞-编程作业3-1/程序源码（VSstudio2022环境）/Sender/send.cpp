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
//告诉编译器链接 ws2_32.lib 静态库。
//ws2_32.lib 是 Windows 平台上 Winsock 的静态库，
//提供了网络编程所需的函数（如 socket、bind、sendto 等）。
using namespace std;

//-------------------------------
//作者 2210529 罗瑞
// 2024.11.20
//-------------------------------

//设置通信的IP和端口号
std::string IPServer = "127.0.0.1";
std::string IPClient = "127.0.0.1";
unsigned short SERVERPORT = 20000;
unsigned short CLIENTPORT = 40000;

//一次最多支持传输0.5GB大小的文件
#define UDP_DATAGRAM_SIZE (8 * 1024) //UDP报文数据大小设置(unsigned short类型变量即可控制)

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
            printf("%s", "UDP校验正确");
            return true;
        }
        return false;
    }
};

/* SEND */
//发送处理函数，计算校验和并发送到addr地址
int sendDatagram(UDP_DATAGRAM& datagram, SOCKET& sock, SOCKADDR_IN& addr)
{
    cout << "SENDING\n";
    datagram.calculate_checksum();
    return sendto(sock, (char*)&datagram, sizeof(UDP_DATAGRAM), 0, (struct sockaddr*)&addr, sizeof(sockaddr));
}
//四个状态位的定义,让不同的状态组合都可以用四位标志变量表示，
// 实际上用了2字节大小的unsigned short类型变量state来记录
//使用亦或逻辑即可直接按位设置
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
/* 超时重传 */
void handleTimeout(UDP_DATAGRAM& datagram, SOCKET sock, SOCKADDR_IN ServerAddr, LARGE_INTEGER& clockstart, int& times)
{
    times++;
    cout << "报文重传 " << datagram.getseq() << " 第 " << times << " 次\n";
    QueryPerformanceCounter(&clockstart);

    sendDatagram(datagram, sock, ServerAddr);
}



// 全局变量
static LARGE_INTEGER timeStart, timeEnd, frequency;
static bool isTimeout = false; // 超时标志
static bool isTimerActive = true; // 计时器是否激活
static int elapsedTicks = 1; // 用于记录并打印经过的时间
static int resendCount = 0; // 记录重传次数
static const int TIMEOUT_LIMIT = 5; // 超时重传限制次数

// 定时器控制标志
static bool isTimerRunning = true;

// 模拟的外部资源
UDP_DATAGRAM datagram0;
SOCKET sock0;
SOCKADDR_IN ServerAddr0;

// 初始化定时器
void initializeTimer() {
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&timeStart);
}

// 打印时间信息
void printElapsedTime(double elapsedTime) {
    if (elapsedTime > elapsedTicks) {
        std::cout << "Elapsed time: " << elapsedTicks << "s\n";
        elapsedTicks++;
    }
}

// 重置计时器
void resetTimer() {
    QueryPerformanceCounter(&timeStart);
    elapsedTicks = 1;
}

// 计时器逻辑
void timerLogic() {
    while (isTimerRunning) {
        if (!isTimerActive) continue; // 如果计时器未激活，跳过逻辑

        // 获取当前时间
        QueryPerformanceCounter(&timeEnd);
        double elapsedTime = static_cast<double>(timeEnd.QuadPart - timeStart.QuadPart) / frequency.QuadPart;

        // 打印时间信息
        printElapsedTime(elapsedTime);

        // 检查是否超时
        if (elapsedTime >= TIMELIMIT) {
            isTimeout = true;
            std::cout << "----------------超时----------------" << std::endl;

            // 调用超时处理函数
            handleTimeout(datagram0, sock0, ServerAddr0, timeStart, resendCount);

            // 重置计时器
            resetTimer();

            // 检查是否达到重传限制
            if (++resendCount >= TIMEOUT_LIMIT) {
                std::cout << "----------------重传超限----------------" << std::endl;
                exit(EXIT_FAILURE); // 程序退出
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

/* 停等协议 */
bool sendAndwait(UDP_DATAGRAM& datagram, UDP_DATAGRAM& recv_datagram, SOCKET sock, SOCKADDR_IN ServerAddr, SOCKADDR_IN ClientAddr)
{
    sendDatagram(datagram, sock, ServerAddr);
    datagram.showseq();
    LARGE_INTEGER clockstart, clockend;
    QueryPerformanceCounter(&clockstart);
    // 传递变量
    timeStart = clockstart;
    isTimerActive = 1;
    datagram0 = datagram;
    sock0 = sock;
    ServerAddr0 = ServerAddr;
    times0 = 0;

    printf("%s", "停等协议……\n");
    while (1)
    {
        recvDatagram(recv_datagram, sock, ServerAddr);
        if (recv_datagram.getstate() & REQ)
        {
            printf("差错重传\n");
            sendDatagram(datagram, sock, ServerAddr);
            datagram.showseq();
            QueryPerformanceCounter(&timeStart);
            continue;
        }
        //检查停等回复
        else if (recv_datagram.getack() == datagram.getseq())
        {
            printf("%s", "此UDP报文发送且接收成功!!!");
            isTimerActive = 0;
            return 1;
        }
    }
    cout << "[error]发送失败\n";
    return 0;
}


/* 读取文件 */
char* readFile(char* fileName, unsigned short& whole, unsigned short& rest, unsigned int& size)
{
    // 打开文件
    ifstream inFile(fileName, ios::in | ios::binary);
    if (!inFile.is_open())
    {
        cout << "[error] 文件打开失败: " << fileName << endl;
        return nullptr;
    }

    // 获取文件大小
    inFile.seekg(0, ios::end);
    size = static_cast<unsigned int>(inFile.tellg());
    if (size == 0)
    {
        cout << "[error] 文件为空: " << fileName << endl;
        inFile.close();
        return nullptr;
    }

    // 返回文件开头
    inFile.seekg(0, ios::beg);

    // 分配内存
    char* filedata = new (nothrow) char[size];
    if (!filedata)
    {
        cout << "[error] 内存分配失败，文件大小: " << size << " 字节" << endl;
        inFile.close();
        return nullptr;
    }

    // 读取文件内容
    inFile.read(filedata, size);
    if (!inFile)
    {
        cout << "[error] 文件读取失败，可能读取的字节数不足: " << inFile.gcount() << " 字节" << endl;
        delete[] filedata;
        inFile.close();
        return nullptr;
    }

    // 计算完整报文个数及剩余字节
    whole = size / UDP_DATAGRAM_SIZE;
    rest = size % UDP_DATAGRAM_SIZE;

    // 关闭文件
    inFile.close();
    cout << "[info] 文件读取成功，大小: " << size << " 字节" << endl;
    return filedata;
}


/* 请求传输 */
bool headfile(char* fileName, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr, unsigned short& num_udp, unsigned short& rest, unsigned int& size)
{
    unsigned int t = strlen(fileName);
    // 拆分文件名
    while (fileName[t] != '/' && t > 0)
        t--;
    // 拼接total和文件名
    char fileInfo[1024];  // 用于存储 total + 文件名 信息
    if (rest % 2 != 0)
        rest += 1;
    // 计算报文总数
    int total = (rest == 0 ? num_udp : num_udp + 1);
    cout << "总报文数: " << total << endl;
    sprintf(fileInfo, "%d %s %d", total, fileName + t + 1, rest);  // 格式化为 "total:<total> <filename>"

    // 将拼接的 total 和文件名信息、rest信息存入报文
    strcpy(send_DataGram.getmessage(), fileInfo);
    
    //报文准备发送
    memset(&send_DataGram, 0, sizeof(UDP_DATAGRAM));
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    send_DataGram.setseq(++nowseq);
    //seq=4
    send_DataGram.setstate(SYN);
    send_DataGram.setstate(REQ);
    // 请求传输的初始报文
    sendAndwait(send_DataGram, respond, sock, ServerAddr, ClientAddr);
}

//COPY//
/* 发送数据报 */
bool sendFileData(unsigned short& rest, unsigned short& num_udp, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr, UDP_DATAGRAM respond, char* fileContent)
{
    isTimerActive = 1;
    thread t1(timerLogic);
    for (int i = 0; i < num_udp; i++)
    {
        UDP_DATAGRAM data;
        memset(&data, 0, sizeof(UDP_DATAGRAM));
        // 确保所有类型数据正确复制，所以使用内存copy
        memcpy(data.getmessage(), fileContent + (i * UDP_DATAGRAM_SIZE), UDP_DATAGRAM_SIZE);
        data.setseq(++nowseq);
        data.setlength(unsigned short(UDP_DATAGRAM_SIZE) + 8);//加上伪首部8字节
        if (sendAndwait(data, respond, sock, ServerAddr, ClientAddr))
        {
            cout << "*";
        }
        else
        {
            cout << "[error]传输失败!\n";
            t1.join();
            isTimerActive = 0;
            return 0;
        }
    }
    // 处理非整数据
    UDP_DATAGRAM last;
    memset(&last, 0, sizeof(UDP_DATAGRAM));
    last.setseq(++nowseq);
    // 检查rest是否为奇数
    if (rest % 2 != 0)
    {
        // rest 是奇数，需要增加一个全零字节
        memcpy(last.getmessage(), fileContent + (num_udp * UDP_DATAGRAM_SIZE), rest);
        last.getmessage()[rest] = 0;  // 添加一个全零字节
        last.setlength(rest + 8 + 1);  // 更新length字段，包括新增的字节
    }
    else
    {
        // 如果rest是偶数，直接复制数据
        memcpy(last.getmessage(), fileContent + (num_udp * UDP_DATAGRAM_SIZE), rest);
        last.setlength(rest + 8);  // 设置正常的length
    }

    if (sendAndwait(last, respond, sock, ServerAddr, ClientAddr)) {
        isTimerActive = 0;
        t1.join();
        return 1;
    }
    else
    {
        cout << "[error]传输失败!\n";
        isTimerActive = 0;
        t1.join();
        return 0;
    }

}



//COPY//
/* 断连 */
int disConnect(SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    // 重置报文
    memset(&send_DataGram, 0, sizeof(UDP_DATAGRAM));
    memset(&respond, 0, sizeof(UDP_DATAGRAM));
    //设置标志位FIN
    send_DataGram.setstate(FIN);
    send_DataGram.setseq(++nowseq);
    printf("%s", "第一次挥手");
    // 发送报文并等待确认
    if (sendAndwait(send_DataGram, respond, sock, ServerAddr, ClientAddr))
    {
        memset(&send_DataGram, 0, sizeof(UDP_DATAGRAM));
        printf("%s", "第三次挥手");
        // 设置标志位ACK
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

/// 读取文件内容并返回内容和一些元数据
char* prepareFileContent(char* fileName, unsigned short& num_udp, unsigned short& rest, unsigned int& size) {
    return readFile(fileName, num_udp, rest, size);
}

// 初始化文件传输请求
bool initiateFileTransfer(char* fileName, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr, unsigned short num_udp, unsigned short rest, unsigned int size) {
    if (!headfile(fileName, sock, ServerAddr, ClientAddr, num_udp, rest, size)) {
        cout << "[error]文件传输请求失败\n";
        return false;
    }
    return true;
}

// 发送文件数据
bool sendFileDataChunks(unsigned short rest, unsigned short num_udp, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr, char* fileContent) {
    if (!sendFileData(rest, num_udp, sock, ServerAddr, ClientAddr, respond, fileContent)) {
        cout << "[error]文件传输失败\n";
        return false;
    }
    return true;
}

// 主发送文件函数
int sendFile(char* fileName, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr) {
    unsigned short num_udp, rest;
    unsigned int size;

    // 读取文件内容并获取相关元数据
    char* fileContent = prepareFileContent(fileName, num_udp, rest, size);
    if (fileContent == NULL) {
        return 0; // 读取文件失败
    }

    // 发起 文件头 传输请求
    if (!initiateFileTransfer(fileName, sock, ServerAddr, ClientAddr, num_udp, rest, size)) {
        return 0; // 请求发送失败
    }

    // 发送文件拆分好的UDP数据
    if (!sendFileDataChunks(rest, num_udp, sock, ServerAddr, ClientAddr, fileContent)) {
        return 0; // 发送文件数据失败
    }

    return 1; // 文件传输成功
}

/* 增加发送序列号 */
void incrementSeqNumber()
{
    ++nowseq;
}

// 发送并确认数据报
bool sendAndReceiveAck(UDP_DATAGRAM& request, UDP_DATAGRAM& response, SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    if (!sendAndwait(request, response, sock, ServerAddr, ClientAddr)) {
        return false;  // 发送失败或确认失败
    }
    return true;  // 成功发送并收到确认
}

// 第一次握手：发送SYN报文并等待ACK
bool firstHandshake(SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    UDP_DATAGRAM request, response;
    memset(&request, 0, sizeof(UDP_DATAGRAM));
    memset(&response, 0, sizeof(UDP_DATAGRAM));

    // 设置SYN标志，发送请求
    request.setstate(SYN);
    ++nowseq;  // 增加序列号
    request.setseq(nowseq);
    printf("%s", "第一次握手:\n");

    // 发送SYN并等待ACK
    return sendAndReceiveAck(request, response, sock, ServerAddr, ClientAddr);
}

// 第二次握手：接收服务器的响应并发送ACK确认
bool secondHandshake(SOCKET& sock, SOCKADDR_IN& ServerAddr, UDP_DATAGRAM& respond)
{
    UDP_DATAGRAM request;
    memset(&request, 0, sizeof(UDP_DATAGRAM));

    int t = respond.getseq();  // 获取服务器返回的序列号
    request.setstate(ACK);      // 设置ACK标志
    request.setack(t);          // 设置ACK为服务器的seq
    ++nowseq;                   // 增加序列号
    request.setseq(nowseq);     // 设置本地seq
    printf("%s", "第三次握手:\n");

    // 发送ACK确认报文
    sendDatagram(request, sock, ServerAddr);
    request.showseq();  // 显示当前序列号

    return true;
}

// 建立连接：执行三次握手过程
bool initateConnection(SOCKET& sock, SOCKADDR_IN& ServerAddr, SOCKADDR_IN& ClientAddr)
{
    UDP_DATAGRAM respond;

    // 第一次握手
    if (!firstHandshake(sock, ServerAddr, ClientAddr)) {
        return false;  // 第一次握手失败
    }

    // 第二次握手：接收服务器响应并发送确认
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
        printf("%s", "[error]WSAStartup错误!\n");
        return 0;
    }
    //与本地IP绑定
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    unsigned long l = 1;
    ioctlsocket(sock, FIONREAD, &l);
    //非阻塞模式

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

    // 建立连接
    if (initateConnection(sock, ServerAddr, ClientAddr))
    {
        string a = "输入单字母T传输文件；\n输入单字母Q退出\n";
        cout << "三次握手连接成功";
        while (1)
        {
            char m;
            cout << a;
            cin >> m;

            if (m == 'q')
            {
                // 断开连接
                disConnect(sock, ServerAddr, ClientAddr);
                break;
            }
            else {
                //选择发送文件
                //cin.getline(file_path,200);

                strcpy(file_path, "D:/28301/Desktop/new/helloworld.txt");
                int ans = 0;
                ans = sendFile(file_path, sock, ServerAddr, ClientAddr);
                if (ans)
                {
                    cout << "文件传输成功";
                }
            }

            nowseq = 1; // 重置
        }
    }
    else
        cout << "[error]\n";
    closesocket(sock);
    WSACleanup();
    return 0;
}



