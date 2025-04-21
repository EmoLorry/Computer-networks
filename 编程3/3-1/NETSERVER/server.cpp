#include "Server.h"

#define SERVER_IP "127.0.0.1"  // 服务器IP地址 
#define SERVER_PORT 9528  // 服务器端口号
#define PACKET_LENGTH 10240
#define BUFFER_SIZE sizeof(Packet)  // 缓冲区大小
using namespace std;
SOCKADDR_IN socketAddr;  // 服务器地址
SOCKADDR_IN addrClient;  // 客户端地址
SOCKET socketServer;  // 服务器套接字

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
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);

	// 打印时间
	cout << ss.str();

	// 恢复默认文本颜色
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void printPacketMessage(Packet* pkt) {  // 打印数据包信息 

	// 获取标准输出句柄
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// 设置文本颜色为蓝色
	SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);

	cout << "Packet size=" << pkt->len << " Bytes  FLAG=" << pkt->FLAG;
	cout << " seqNumber=" << pkt->seq << " ackNumber=" << pkt->ack;
	cout << " checksum=" << pkt->checksum << " windowLength=" << pkt->window << endl;

	// 恢复默认文本颜色
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void printReceivePacketMessage(Packet* pkt) {
	cout << "接收数据包的信息：";
	printPacketMessage(pkt);
}

int fromLen = sizeof(SOCKADDR);
int waitSeq;  // 等待的数据包序列号
int err;  // socket错误提示
int packetNum;  // 发送数据包的数量
int fileSize;  // 文件大小
unsigned int recvSize;  // 累积收到的文件位置
char* fileName;
char* fileBuffer;

default_random_engine randomEngine;
uniform_real_distribution<float> randomNumber(0.0, 1.0);  // 自己设置丢包

void initSocket() {
	WSADATA wsaData;  // 存储被WSAStartup函数调用后返回的Windows Sockets数据  
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);  // 指定版本 2.2
	if (err != NO_ERROR) {
		cout << "WSAStartup failed with error: " << err << endl;
		return;
	}
	socketServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // 通信协议：IPv4 Internet协议; 套接字通信类型：TCP链接;  协议特定类型：某些协议只有一种类型，则为0

	u_long mode = 1;
	ioctlsocket(socketServer, FIONBIO, &mode); // 设置阻塞模式，等待来自客户端的数据包

	socketAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);  // 0.0.0.0, 即本机所有IP
	socketAddr.sin_family = AF_INET;  // 使用IPv4地址
	socketAddr.sin_port = htons(SERVER_PORT);  // 端口号


	err = bind(socketServer, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));  // 绑定到socketServer
	if (err == SOCKET_ERROR) {
		err = GetLastError();
		cout << "Could not bind the port " << SERVER_PORT << " for socket. Error message: " << err << endl;
		WSACleanup();
		return;
	}
	else {
		printTime();
		cout << "服务器启动成功，等待客户端建立连接" << endl;
	}
}


void sendSYNACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setSYNACK();
	sendto(socketServer, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "发送带ACK和SYN的数据包..." << endl;
}

bool receiveSYN(Packet* recvPkt) {

	// 设置套接字为阻塞模式，避免非阻塞行为
	unsigned long mode = 0;
	ioctlsocket(socketServer, FIONBIO, &mode);  // 设置为阻塞模式

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
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
	ioctlsocket(socketServer, FIONBIO, &mode);  // 设置为阻塞模式

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0 && (recvPkt->FLAG & 0x4)) {
		printTime();
		cout << "收到客户端的ACK确认数据包..." << endl;
		return true;
	}
	return false;
}

void connect()
{
	int state = 0;  // 标识目前握手的状态
	bool connectionEstablished = 0;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;
	while (!connectionEstablished)
	{
		// 第一次握手: 等待客户端发送SYN
		if (receiveSYN(recvPkt)) {
			sendSYNACK();  // 第二次握手: 发送SYN-ACK
			// 第三次握手: 等待客户端确认ACK
			if (receiveACK(recvPkt)) {
				connectionEstablished = 1;
			}
		}
	}
	printTime();
	cout << "三次握手完成，连接已建立，开始文件传输..." << endl;
	cout << "----------------------------------------------------------------------" << endl;

}
bool receiveFIN(Packet* recvPkt) {

	// 设置套接字为阻塞模式，避免非阻塞行为
	unsigned long mode = 0;
	ioctlsocket(socketServer, FIONBIO, &mode);  // 设置为阻塞模式

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
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
	sendto(socketServer, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "发送带ACK的数据包..." << endl;
}
void sendFINACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setFINACK();
	sendto(socketServer, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "发送带ACK和FIN的数据包..." << endl;
}
void disconnect() {
	int state = 0;  // 标识目前挥手的状态
	bool flag = 1;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;
	receiveFIN(recvPkt);//第一次挥手：接收FIN
	sendACK();  // 第二次挥手: 发送ACK
	sendFINACK();// 第三次挥手: 发送ACK、FIN
	receiveACK(recvPkt);//结束挥手
}
void saveFile() {
	string filePath = "D:/28301/Desktop/get/";  // 保存路径
	for (int i = 0; fileName[i]; i++)filePath += fileName[i];
	ofstream fout(filePath, ios::binary | ios::out);

	fout.write(fileBuffer, fileSize); // 这里还是size,如果使用string.data或c_str的话图片不显示，经典深拷贝问题
	fout.close();
}

void receiveHead(Packet* recvPkt, Packet* sendPkt)
{
	// 设置套接字为阻塞模式，避免非阻塞行为
	unsigned long mode = 0;
	ioctlsocket(socketServer, FIONBIO, &mode);  // 设置为阻塞模式

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0) {
		printReceivePacketMessage(sendPkt);
		if (isCorrupt(recvPkt)) {  // 检测出数据包损坏
			printTime();
			cout << "收到一个损坏的数据包，向发送端发送 ack=" << waitSeq << " 数据包" << endl;

			sendPkt->ack = waitSeq - 1;
			sendPkt->setACK();
			sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
		}
		//接收方收到带有 HEAD 标志位的数据包后，会解析其中的文件名和文件大小信息，为后续的文件接收做好准备。
		//接收方会分配足够的缓冲区来存储接收到的文件数据，并准备好接收后续的数据包。
		if (recvPkt->FLAG & 0x8) {  // HEAD=1
			fileSize = recvPkt->len;
			fileBuffer = new char[fileSize];
			fileName = new char[128];
			memcpy(fileName, recvPkt->data, strlen(recvPkt->data) + 1);

			//如果 fileSize % PACKET_LENGTH 不为 0，说明文件大小不是数据包大小的整数倍，因此需要多发送一个数据包来传输剩余的数据。
			//如果 fileSize% PACKET_LENGTH 为 0，说明文件大小正好是数据包大小的整数倍，不需要额外的数据包。
			packetNum = fileSize % PACKET_LENGTH ? fileSize / PACKET_LENGTH + 1 : fileSize / PACKET_LENGTH;

			// 三元运算符 ? : 是一个条件运算符，用于根据条件选择两个表达式中的一个。
			// 语法：condition ? expression1 : expression2
			//		如果 condition 为真（非零），则整个表达式的值为 expression1。
			//		如果 condition 为假（零），则整个表达式的值为 expression2。


			printTime();
			cout << "收到来自发送端的文件头数据包，文件名为: " << fileName;
			cout << "。文件大小为: " << fileSize << " Bytes，总共需要接收 " << packetNum << " 个数据包";
			cout << "，等待发送文件数据包..." << endl;

			waitSeq++;

			sendPkt->ack = waitSeq - 1;
			sendPkt->setACK();
			sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));

		}
		else {
			printTime();
			cout << "收到的数据包不是文件头，等待发送端重传..." << endl;
		}
	}
}
void receiveData(Packet* recvPkt, Packet* sendPkt)
{
	// 设置套接字为阻塞模式，避免非阻塞行为
	unsigned long mode = 0;
	ioctlsocket(socketServer, FIONBIO, &mode);  // 设置为阻塞模式

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0) {
		printReceivePacketMessage(recvPkt);
		if (isCorrupt(recvPkt)) {  // 检测出数据包损坏
			printTime();
			// 获取标准输出句柄
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			// 设置文本颜色为红色
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
			cout << "主动损坏数据包，向发送端发送 ack=" << waitSeq << " 数据包" << endl;
			// 恢复默认文本颜色
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

			sendPkt->ack = waitSeq - 1;
			sendPkt->setACK();
			sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
		}
		if (recvPkt->seq == waitSeq) {  // 收到了目前等待的包
			if (randomNumber(randomEngine) >= 0.01) {
				memcpy(fileBuffer + recvSize, recvPkt->data, recvPkt->len);
				recvSize += recvPkt->len;

				printTime();
				cout << "收到第 " << recvPkt->seq << " 号数据包，向发送端发送 ack=" << waitSeq << endl;

				waitSeq++;

				sendPkt->ack = waitSeq - 1;
				sendPkt->setACK();
				sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
			}
			else {

				if (randomNumber(randomEngine) >= 0.5)
				{
					// 获取标准输出句柄
					HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
					// 设置文本颜色为红色
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
					cout << "主动丢包" << endl;
					// 恢复默认文本颜色
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
				}
				else
				{
					printTime();
					// 获取标准输出句柄
					HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
					// 设置文本颜色为红色
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
					cout << "主动损坏数据包，向发送端发送 ack=" << waitSeq << " 数据包" << endl;
					// 恢复默认文本颜色
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);


					sendPkt->ack = waitSeq - 1;
					sendPkt->setACK();
					sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
				}
			}
		}
		else {  // 不是目前等待的数据包
			printTime();
			cout << "收到第 " << recvPkt->seq << " 号数据包，但并不是需要的数据包，向发送端发送 ack=" << waitSeq << endl;
		}
	}
}


void receiveFile() {
	int state = 0;
	bool flag = 1;
	waitSeq = 0;
	recvSize = 0;
	Packet* recvPkt = new Packet;
	Packet* sendPkt = new Packet;

	receiveHead(recvPkt, sendPkt);
	while (recvSize != fileSize)
	{
		receiveData(recvPkt, sendPkt);
	}
	printTime();
	cout << "文件接收完毕..." << endl;
	cout << "----------------------------------------------------------------------" << endl;

	// 保存文件
	saveFile();

	flag = 0;
	disconnect();

}

int main() {
	// 初始化服务器socket
	initSocket();

	// 建立连接
	connect();

	// 开始接收文件
	receiveFile();

	// 断连完成后，关闭socket
	err = closesocket(socketServer);
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