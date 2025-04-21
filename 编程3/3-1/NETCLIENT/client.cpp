#include "Client.h"

#define SERVER_IP "127.0.0.1"  // 服务器的IP地址
#define SERVER_PORT 9528	// 端口
#define BUFFER_SIZE sizeof(Packet)  // 缓冲区大小
#define WINDOW_SIZE 20
#define PACKET_LENGTH 10240
#define MAX_TIME 2 * CLOCKS_PER_SEC  // 超时重传

#include <windows.h>

SOCKADDR_IN socketAddr;  // 服务器地址
SOCKET socketClient;  // 客户端套接字

stringstream ss;
SYSTEMTIME sysTime = { 0 };
void printTime() {  // 打印系统时间
	ss.clear();
	ss.str("");
	GetSystemTime(&sysTime);
	ss << "[" << sysTime.wYear << "/" << sysTime.wMonth << "/" << sysTime.wDay << " " << sysTime.wHour + 8 << ":" << sysTime.wMinute << ":" << sysTime.wSecond << ":" << sysTime.wMilliseconds << "]";

	// 获取标准输出句柄
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// 设置文本颜色为绿色
	SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);

	// 打印时间
	cout << ss.str();

	// 恢复默认文本颜色
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);


}

void printPacketMessage(Packet* pkt) {  // 打印数据包信息 

	// 获取标准输出句柄
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// 设置文本颜色为蓝色
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

	cout << "Packet size=" << pkt->len << " Bytes  FLAG=" << pkt->FLAG;
	cout << " seqNumber=" << pkt->seq << " ackNumber=" << pkt->ack;
	cout << " checksum=" << pkt->checksum << " windowLength=" << pkt->window << endl;

	// 恢复默认文本颜色
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void printSendPacketMessage(Packet* pkt) {

	cout << "UDP_Packet_info：";
	printPacketMessage(pkt);
}

int fromLen = sizeof(SOCKADDR);
int packetNum;  // 发送数据包的数量
int fileSize;  // 文件大小
int sendSeq;  // 发送数据包序列号
int err;  // socket错误提示
char* filePath;
char* fileName;
char* fileBuffer;

void initSocket() {
	WSADATA wsaData;
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);  // 版本 2.2 
	if (err != NO_ERROR)
	{
		cout << "WSAStartup failed with error: " << err << endl;
		return;
	}
	socketClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	socketAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons(SERVER_PORT);

	printTime();
	cout << "客户端初始化成功，准备与服务器建立连接..." << endl;
}

void sendSYNACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setSYNACK();
	sendto(socketClient, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "发送带ACK和SYN的数据包..." << endl;
}

bool receiveSYN(Packet* recvPkt) {

	// 设置套接字为阻塞模式，避免非阻塞行为
	unsigned long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);  // 设置为阻塞模式

	int err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0 && (recvPkt->FLAG & 0x2)) {
		printTime();
		cout << "收到客户端的SYN数据包，准备第二次握手..." << endl;
		return true;
	}
	return false;
}

bool receiveACK(Packet* recvPkt) {

	// 设置套接字为阻塞模式，避免非阻塞行为
	unsigned long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);  // 设置为阻塞模式

	int err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0 && (recvPkt->FLAG & 0x4)) {
		printTime();
		cout << "收到客户端的ACK确认数据包..." << endl;
		return true;
	}
	return false;
}

bool receiveFIN(Packet* recvPkt) {

	// 设置套接字为阻塞模式，避免非阻塞行为
	unsigned long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);  // 设置为阻塞模式

	int err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0 && (recvPkt->FLAG & 0x1)) {
		printTime();
		cout << "收到客户端的FIN数据包..." << endl;
		return true;
	}
	return false;
}
void sendACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setACK();
	sendto(socketClient, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "发送带ACK的数据包..." << endl;
}
void sendFINACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setFINACK();
	sendto(socketClient, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "发送带ACK和FIN的数据包..." << endl;
}


void sendSYN(Packet* pktToSend)
{
	pktToSend->setSYN();
	sendto(socketClient, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "发送带SYN的数据包..." << endl;
}

void receiveSYNACK(Packet* recvPkt, Packet* sendPkt) {

	// 设置套接字为阻塞模式，避免非阻塞行为
	unsigned long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);  // 设置为阻塞模式

	int err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if ((recvPkt->FLAG & 0x2) && (recvPkt->FLAG & 0x4)) {  // SYN=1, ACK=1
		printTime();
		cout << "开始第三次握手，向服务器发送ACK=1的数据包..." << endl;

		// 第三次握手，向服务器发送ACK=1的数据包，告知服务器自己接收能力正常
		sendPkt->setACK();
		sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));

		printTime();
		cout << "三次握手结束，确认已建立连接，开始文件传输..." << endl;
		cout << "----------------------------------------------------------------------" << endl;
	}
	else {
		printTime();
		cout << "第二次握手阶段收到的数据包有误，握手失败..." << endl;
	}
}
void connect() {
	int state = 0;  // 标识目前握手的状态
	bool flag = 1;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;

	sendSYN(sendPkt);// 第一次握手，向服务器发送SYN=1的数据包
	receiveSYNACK(recvPkt, sendPkt);//

}

void sendFIN(Packet* sendPkt)
{
	printTime();
	cout << "开始第一次挥手，向服务器发送FIN=1的数据包..." << endl;

	// 第一次挥手，向服务器发送FIN=1的数据包
	sendPkt->setFIN();
	sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
}
void receiveFINACK(Packet* recvPkt, Packet* sendPkt)
{
	// 设置套接字为阻塞模式，避免非阻塞行为
	unsigned long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);  // 设置为阻塞模式

	err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err >= 0) {
		if ((recvPkt->FLAG & 0x1) && (recvPkt->FLAG & 0x4)) {  // ACK=1, FIN=1
			printTime();
			cout << "收到了来自服务器第三次挥手FIN&ACK数据包，开始第四次挥手，向服务器发送ACK=1的数据包..." << endl;

			// 第四次挥手，向服务器发送ACK=1的数据包，通知服务器确认断开连接
			sendPkt->setFINACK();
			sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));

			printTime();
			cout << "四次挥手结束，确认已断开连接..." << endl;
		}
		else {
			printTime();
			cout << "第三次挥手阶段收到的数据包有误，重新开始第三次挥手..." << endl;
		}
	}
}
void disconnect() {
	int state = 0;  // 标识目前挥手的状态	
	bool flag = 1;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;

	sendFIN(sendPkt);
	receiveACK(recvPkt);
	receiveFINACK(recvPkt, sendPkt);

}

void printOverTime()
{
	printTime();

	// 获取标准输出句柄
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// 设置文本颜色为红色
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED);

	cout << "超时！请重新发送此数据包" << endl;

	// 恢复默认文本颜色
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}
void sendData(Packet* sendPkt)
{
	err = sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
	if (err == SOCKET_ERROR) {

		// 获取标准输出句柄
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

		// 设置文本颜色为红色
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED);

		cout << "sendto failed with error: " << WSAGetLastError() << endl;

		// 恢复默认文本颜色
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	}
}
void waitACK(Packet* recvPkt, Packet* sendPkt)
{
	clock_t start = clock();  // 记录发送时间，超时重传

	// 等待接收ACK信息，验证acknowledge number
	while (true) {
		// 设置套接字为非阻塞模式
		unsigned long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);  // 设置为非阻塞模式

		while (recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen) <= 0) {
			if (clock() - start > MAX_TIME) {
				printOverTime();
				sendData(sendPkt);
				start = clock(); // 重设开始时间
			}
		}
		// 三个条件要同时满足，这里判断用packet的seq
		if ((recvPkt->FLAG & 0x4) && (recvPkt->ack == sendPkt->seq)) {
			sendSeq++;
			break;
		}
		else
		{
			printTime();

			// 获取标准输出句柄
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

			// 设置文本颜色为红色
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED);

			cout << "数据包损坏，即将重发" << endl;

			// 恢复默认文本颜色
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

			sendData(sendPkt);
			start = clock(); // 重设开始时间
		}
	}
}
void sendPacket(Packet* sendPkt) {
	Packet* recvPkt = new Packet;

	// 检查一下文件的各个内容
	printSendPacketMessage(sendPkt);
	sendData(sendPkt);

	waitACK(recvPkt, sendPkt);
}






void readFile() {
	ifstream f(filePath, ifstream::in | ios::binary);  // 以二进制方式打开文件
	if (!f.is_open()) {
		// 获取标准输出句柄
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

		// 设置文本颜色为红色
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED);

		cout << "文件无法打开！" << endl;

		// 恢复默认文本颜色
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		return;
	}
	f.seekg(0, std::ios_base::end);  // 将文件流指针定位到流的末尾
	fileSize = f.tellg();
	packetNum = fileSize % PACKET_LENGTH ? fileSize / PACKET_LENGTH + 1 : fileSize / PACKET_LENGTH;
	cout << "文件大小为 " << fileSize << " Bytes，总共要发送 " << packetNum + 1 << " 个数据包" << endl;
	f.seekg(0, std::ios_base::beg);  // 将文件流指针重新定位到流的开始
	fileBuffer = new char[fileSize];
	f.read(fileBuffer, fileSize);
	f.close();
}

void sendFile() {
	sendSeq = 0;
	clock_t start = clock();

	// 先发一个记录文件名的数据包，并设置HEAD标志位为1，表示开始文件传输
	// HEAD 标志位用于标识一个数据包是文件传输的头部信息。
	// 在文件传输开始时，发送方会发送一个带有 HEAD 标志位的数据包，这个数据包包含了文件的基本信息，如文件名和文件大小。
	Packet* headPkt = new Packet;
	printTime();
	cout << "发送文件头数据包..." << endl;
	headPkt->setHEAD(sendSeq, fileSize, fileName);
	headPkt->checksum = checksum((uint32_t*)headPkt);
	sendPacket(headPkt);

	// 开始发送装载文件的数据包
	printTime();
	cout << "开始发送文件数据包..." << endl;
	Packet* sendPkt = new Packet;
	for (int i = 0; i < packetNum; i++) {
		if (i == packetNum - 1) {  // 最后一个包
			sendPkt->setTAIL();
			sendPkt->fillData(sendSeq, fileSize - i * PACKET_LENGTH, fileBuffer + i * PACKET_LENGTH);
			sendPkt->checksum = checksum((uint32_t*)sendPkt);
			//TAIL 标志位用于标识文件传输的结束，发送方通过它告知接收方文件传输已完成，接收方可以停止等待新的数据包并进行后续处理。
		}
		else {
			sendPkt->fillData(sendSeq, PACKET_LENGTH, fileBuffer + i * PACKET_LENGTH);
			sendPkt->checksum = checksum((uint32_t*)sendPkt);
		}
		sendPacket(sendPkt);
		Sleep(20);
	}

	clock_t end = clock();
	printTime();
	cout << "文件发送完毕..." << endl;
	cout << "传输时间为：" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
	cout << "吞吐率为：" << ((float)fileSize) / ((end - start) / CLOCKS_PER_SEC) << " Bytes/s " << endl;
	cout << "----------------------------------------------------------------------" << endl;
}

int main() {
	// 初始化客户端
	initSocket();

	// 与服务器端建立连接
	connect();

	cout << "请输入您要传输的文件所在路径(绝对路径)：";
	filePath = new char[128];
	cin >> filePath;

	// 将 char* 转换为 string，便于操作
	string path(filePath);

	// 找到路径中最后一个反斜杠的位置
	size_t pos = path.find_last_of("\\/");  // 支持反斜杠和正斜杠

	// 提取文件名部分（反斜杠之后的部分）
	string fileNameStr = path.substr(pos + 1);  // 提取文件名

	// 将提取到的文件名复制到全局变量 fileName
	fileName = new char[fileNameStr.length() + 1]; // +1 for null terminator
	strcpy(fileName, fileNameStr.c_str());  // 使用 strcpy 将字符串复制到 fileName

	readFile();

	// 开始发送文件
	sendFile();

	// 主动断开连接
	disconnect();

	// 断连完成后，关闭socket
	err = closesocket(socketClient);
	if (err == SOCKET_ERROR) {
		cout << "Close socket failed with error: " << WSAGetLastError() << endl;
		return 1;
	}

	// 清理退出
	printTime();
	cout << "程序退出..." << endl;
	WSACleanup();

	system("pause");
	return 0;
}