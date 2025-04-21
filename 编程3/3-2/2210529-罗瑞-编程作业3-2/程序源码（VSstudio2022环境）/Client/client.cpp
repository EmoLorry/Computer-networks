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

//宏定义端口以及IP
#define SERVER_ip "127.0.0.1"
#define CLIENT_ip "127.0.0.1"
#define SERVER_port 10000       // 服务器端口
#define CLIENT_port 40000     // 客户端端口
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
#define timelimit_udp 150 //窗口内的报文0.15s未确认则重传
//全局变量滑动窗的序列起止位置、发送序列号、udp数据报个数以及最后一个数据报的字节数
int windowtop = 0;
int windowbase = 0;
int sendseq = 0;
pack udp_num;
pack rest;

// 声明线程池，用于管理所有分片发送线程
std::vector<std::thread> threads;
//初始化缓冲区，最大支持范围内可能用到的缓冲空间
char buffer[65536][UDP_DATAGRAM_SIZE];
//初始化数据报组接收确认数组
bool acked[65536];
bool isthread[65536];
int accumulate_point = -1;//累积确认指针
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

void print_red_message(int seq) {
    // ANSI 转义序列，红色字体
    const string RED = "\033[31m";
    // ANSI 转义序列，重置颜色
    const string RESET = "\033[0m";

    // 打印带颜色的输出
    cout << RED << "序列为" << seq << "的报文超时未确认，触发重传机制" << RESET << endl;
}

void print_yellow_message(int start,int end) {
    // ANSI 转义序列，黄色字体
    const string RED = "\033[33m";
    // ANSI 转义序列，重置颜色
    const string RESET = "\033[0m";
    cout << RED << "========================滑动窗口右移1位=========================" << endl;
    // 打印带颜色的输出
    cout << "             窗口底部序列: "<< start << "，窗口顶部序列:"<<end<<endl;
    cout << "================================================================" << RESET<< endl;
    cout << endl;
}

//客户端：建连函数和断连函数，采用三次握手和“三次挥手”原则
int connection() {
    UDP_DATAGRAM build_first, build_second, build_third;

    //===================================================================================================
    //第一次握手
    sendseq += 1;
    build_first.setstate(SYN);
    build_first.setseq(sendseq);
    build_first.calculate_checksum(); // 计算并设置校验和

    if (sendto(sock, (char*)&build_first, sizeof(build_first), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
        cout << "建连报文发送错误，错误码: " << WSAGetLastError() << endl;
        return 0;
    }
    else {
        cout << "第一次握手建连报文发送成功" << endl;
    }
    //cout << "第" << "senseq" << "报文发送错误" << endl;
    int start = clock();//记录时间


    //===================================================================================================
    int relimit = 0;//设置最大10次重发
    // 确认第二次握手
    int end; // 当前时间变量
    cout << "建连中：******";
    while (true) {
        end = clock(); // 获取当前时间
        if (end - start > timelimit)
        {
            if (relimit == 10)
                return 0;
            start = clock();//更新计时
            cout << "建连超时，重发第一次握手" << endl;
            relimit++;
            if (sendto(sock, (char*)&build_first, sizeof(build_first), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
                cout << "建连报文发送错误，错误码: " << WSAGetLastError() << endl;
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
    // 第三次握手
    sendseq += 1;
    build_third.setstate(ACK);
    build_third.setseq(sendseq);
    build_third.calculate_checksum(); // 计算并设置校验和
    if (sendto(sock, (char*)&build_third, sizeof(build_third), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
        cout << "建连报文发送错误，错误码: " << WSAGetLastError() << endl;
        return 0;
    }
    else
        return 1;

}

//发送一个数据报的核心函数，处理方式：丢包&差错不会引发确认，最终会触发超时重传，
// 注意每一个窗口内的数据报不能互相影响，每一个数据报的处理都应该是一个线程
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
    udp_now.calculate_checksum(); // 计算并设置校验和
    //=========================================================================================
    //发送数据报
    if (sendto(sock, (char*)&udp_now, sizeof(udp_now), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
        cout << "序列为" << seq << "的数据报文发送错误，错误码: " << WSAGetLastError() << endl;
        return 0;
    }
    //cout << "序列为" << seq << "的数据报文发送成功！"  << endl;

// 注意差错的报文也是未确认，启动超时检测重传机制
    int start = clock(); // 记录发送起始时间

    while (true)
    {
        int end = clock(); // 获取当前时间

        // 等待报文确认，超时重传
        if (end - start > timelimit_udp)
        {
            print_red_message(seq);
            //cout << "序列为" <<  << "的报文超时未确认，触发重传机制" << endl;
            //发送数据报
            if (sendto(sock, (char*)&udp_now, sizeof(udp_now), 0, (SOCKADDR*)&server, l) == SOCKET_ERROR) {
                //cout << "序列为" << seq << "的数据报文发送错误，错误码: " << WSAGetLastError() << endl;
                return 0;
            }
            else
                //cout << "序列为" << seq << "的数据报文发送成功！" << endl;
                start = clock();  // 重置起始时间
        }

        //线程退出的条件
        if (acked[seq])
            return 1; // 返回 1 表示消息发送成功并被确认  

        //Sleep(50);
    }

}

//发送端核心处理函数，处理方式：滑动窗口，这个函数为了和滑动窗口设置函数并行，设置为线程
int sending() {
    // 不断遍历检测，发送滑动窗口内的未确认分片
    while (true) {
        // 检测到滑动窗口完毕后直接退出
        if (windowbase == udp_num - 1 && acked[windowbase])
            break;

        for (int i = windowbase; i <= windowtop; i++) {
            if (!acked[i] && !isthread[i]) { // 检查分片状态，只发送窗口内表示未确认的报文
                // 启动一个线程发送分片,建立过的线程没必要再建立
                threads.emplace_back(std::thread(udpdatagramcopyandsend, i));
                isthread[i] = true;
            }
        }
    }

    // 等待所有发送线程完成
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join(); // 阻塞等待线程结束
        }
    }

    //发送文件传输确认报文
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
            //超时重传
            int timeold = clock();//更新计时
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
        cout << "恭喜，所有数据报文均被正确接收！";
        break;
    }
    }

    return 1; // 发送函数完成
}



    int receive() {
        while (true)
        {
            UDP_DATAGRAM recv;
            memset(&recv, 0, sizeof(UDP_DATAGRAM));
            recvfrom(sock, (char*)&recv, sizeof(UDP_DATAGRAM), 0, (SOCKADDR*)&server, &l);
            if (recv.getstate() & REQ)
            {
                cout << "发送序列为" << recv.getack() << "的报文传输出现数据错误！" << endl;
                continue;
            }

            else if (recv.getstate() & ACK)
            {
                cout << "累积确认，序列号" << recv.getack() << "之前的所有数据报已确认！" << endl;
                for(int j= accumulate_point+1;j<= recv.getack();j++)
                    acked[j] = 1;          // 更新状态，表示累积确认
                accumulate_point = recv.getack();//更新累积确认指针
                cout << "===============================================================" << endl;



            }
            //线程退出条件
            //条件：若滑动窗口到最后，且检查窗口内的是否都已确认
            bool exitjudge = true;
            for (int i = windowbase; i <= udp_num - 1; i++)
                if (!acked[i])
                    exitjudge = false;

            if (exitjudge)
                return 1; // 如果确认号是最后一个分片，退出接收线程
        }
    }

    int disconnection() {
        UDP_DATAGRAM bye_first, bye_second, bye_third;
        return 1;
    }




    // 读取文件数据并进行相应处理，加载到缓冲区
    void dealwithfiledata(char* fileName) {
        // 打开文件
        ifstream inFile(fileName, ios::in | ios::binary);
        if (!inFile.is_open()) {
            cout << "[error] 文件打开失败: " << fileName << endl;
            return;
        }

        // 获取文件大小
        inFile.seekg(0, ios::end);
        unsigned int size = static_cast<unsigned int>(inFile.tellg());
        if (size == 0) {
            cout << "[error] 文件为空: " << fileName << endl;
            inFile.close();
            return;
        }

        // 返回文件开头
        inFile.seekg(0, ios::beg);

        // 计算完整报文个数及剩余字节
        udp_num = size / UDP_DATAGRAM_SIZE;
        rest = size % UDP_DATAGRAM_SIZE;

        //将文件基本信息放到首报文中
        char fileInfo[1024];  // 用于存储 total + 文件名 信息
        unsigned int t = strlen(fileName);
        // 拆分文件名
        while (fileName[t] != '/' && t > 0)
            t--;


        // 读取文件数据到缓冲区
        for (unsigned short i = 1; i < udp_num + 1; ++i) {
            inFile.read(buffer[i], UDP_DATAGRAM_SIZE);
            if (!inFile) {
                cout << "[error] 文件读取失败，读取到第 " << i << " 个数据报时出错" << endl;
                inFile.close();
                return;
            }
        }

        // 读取剩余字节到最后一个数据报
        if (rest > 0) {
            udp_num += 1;//注意udp_num的含义
            inFile.read(buffer[udp_num + 1], rest);
            if (!inFile) {
                cout << "[error] 文件读取失败，读取剩余字节时出错" << endl;
                inFile.close();
                return;
            }
        }

        // 如果剩余字节数为奇数，添加一个全零字节
        if (rest % 2 != 0) {
            buffer[udp_num + 1][rest] = '\0';  // 在剩余数据的后面添加一个零字节
            ++rest;  // 更新剩余字节数
        }

        //将调整rest后的文件基本信息放到首报文中
        // 拼接total和文件名
        sprintf(fileInfo, "%d %s %d", udp_num + 1, fileName + t + 1, rest);  // 格式化为 "<total> <filename><rest>"
        strcpy(buffer[0], fileInfo);

        //加上头报文
        udp_num += 1;

        // 关闭文件
        inFile.close();
        cout << "[info] 文件读取成功，大小: " << size << " 字节，完整数据报个数: " << udp_num
            << "，尾数据报字节数: " << rest << endl;
    }


    //阻塞函数处理滑动窗口，一直循环到所有数据报文发送且接收确认完毕
    void windowsending() {
        while (true) {
            if (acked[windowbase])
            {
                if (windowbase == udp_num - 1)
                    break;//发送序列全部确认完毕
                windowbase++;     // 滑动窗口底部右移一位    
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
            printf("%s", "[error]WSAStartup错误!\n");
            return 0;
        }

        client.sin_addr.S_un.S_addr = inet_addr(CLIENT_ip);
        client.sin_family = AF_INET;
        client.sin_port = htons(CLIENT_port);// 设置服务器端口号
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        u_long mode = 0;  // 1 表示非阻塞模式
        if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
            cout << "[error] 无法设置阻塞模式, 错误码: " << WSAGetLastError() << endl;
            closesocket(sock);
            WSACleanup();
            return -1;
        }

        bind(sock, (SOCKADDR*)&client, sizeof(SOCKADDR));
        if (sock == INVALID_SOCKET) {
            cout << "套接字创建失败" << endl;
            return 0;
        }

        server.sin_addr.S_un.S_addr = inet_addr(SERVER_ip);
        server.sin_family = AF_INET;
        server.sin_port = htons(SERVER_port);

        if (connection())
            cout << "建连成功!" << endl;
        else
        {
            cout << "建连失败！" << endl;
            return 0;
        }




        cout << "请输入要传输的文件的路径" << endl;
        string path;
        cin >> path;

        //D:/28301/Desktop/new/helloworld.txt
        //D:/28301/Desktop/new/2.jpg
        // D:/28301/Desktop/new/1.jpg
        //D:/28301/Desktop/new/3.jpg
        //处理文件，将文件数据加载到缓冲区，并计算UDP数据报个数
        dealwithfiledata(&path[0]);

        //注意windowtop和windowbase都是窗口的位置指针
        //初始化windowtop和windowbase
        windowbase = 0;
        if (udp_num - 1 <= windowlength)
            windowtop = udp_num - 1;
        else
            windowtop = windowlength;

        //初始化acked[udp_num]数组
        for (int i = 0; i < udp_num; i++)
            acked[i] = false;
        for (int i = 0; i < udp_num; i++)
            isthread[i] = false;

        int timestart = clock(); // 开始计时

        // 创建接收线程，负责接收服务器确认消息
        thread recv(receive);
        recv.detach(); // 将线程分离

        // 创建发送线程，负责发送文件分片
        cout << "开始传输文件对应的UDP数据报……" << endl;
        thread send(sending);
        send.detach(); // 将线程分离


        //窗口滑动确认处理“阻塞器”,直到所有报文都被正确接收确认
        windowsending();

        int timeend = clock(); // 结束计时

        // 计算并输出传输性能
        double endtime = (double)(timeend - timestart) / CLOCKS_PER_SEC;
        cout << "文件传输时间" << endtime << "s" << endl;
        cout << "传输吞吐率" << (double)(udp_num) * (sizeof(UDP_DATAGRAM) << 3) / endtime / 1024 << "kb/s" << endl;


        //三次挥手
        if (disconnection())
            cout << "断连成功" << endl;

        closesocket(sock);
        WSACleanup();
        system("pause");
        return 0;
    }