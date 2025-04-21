#include "Server.h"

#define SERVER_IP "127.0.0.1"  // ������IP��ַ 
#define SERVER_PORT 9528  // �������˿ں�
#define PACKET_LENGTH 10240
#define BUFFER_SIZE sizeof(Packet)  // ��������С
using namespace std;
SOCKADDR_IN socketAddr;  // ��������ַ
SOCKADDR_IN addrClient;  // �ͻ��˵�ַ
SOCKET socketServer;  // �������׽���

stringstream ss;
SYSTEMTIME sysTime = { 0 };
void printTime() {  // ��ӡϵͳʱ��
	ss.clear();
	ss.str("");
	GetSystemTime(&sysTime);
	ss << "[" << sysTime.wYear << "/" << sysTime.wMonth << "/" << sysTime.wDay << " " << sysTime.wHour + 8 << ":" << sysTime.wMinute << ":" << sysTime.wSecond << ":" << sysTime.wMilliseconds << "]";
	// ��ȡ��׼������
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// �����ı���ɫΪ��ɫ
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);

	// ��ӡʱ��
	cout << ss.str();

	// �ָ�Ĭ���ı���ɫ
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void printPacketMessage(Packet* pkt) {  // ��ӡ���ݰ���Ϣ 

	// ��ȡ��׼������
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// �����ı���ɫΪ��ɫ
	SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);

	cout << "Packet size=" << pkt->len << " Bytes  FLAG=" << pkt->FLAG;
	cout << " seqNumber=" << pkt->seq << " ackNumber=" << pkt->ack;
	cout << " checksum=" << pkt->checksum << " windowLength=" << pkt->window << endl;

	// �ָ�Ĭ���ı���ɫ
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void printReceivePacketMessage(Packet* pkt) {
	cout << "�������ݰ�����Ϣ��";
	printPacketMessage(pkt);
}

int fromLen = sizeof(SOCKADDR);
int waitSeq;  // �ȴ������ݰ����к�
int err;  // socket������ʾ
int packetNum;  // �������ݰ�������
int fileSize;  // �ļ���С
unsigned int recvSize;  // �ۻ��յ����ļ�λ��
char* fileName;
char* fileBuffer;

default_random_engine randomEngine;
uniform_real_distribution<float> randomNumber(0.0, 1.0);  // �Լ����ö���

void initSocket() {
	WSADATA wsaData;  // �洢��WSAStartup�������ú󷵻ص�Windows Sockets����  
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);  // ָ���汾 2.2
	if (err != NO_ERROR) {
		cout << "WSAStartup failed with error: " << err << endl;
		return;
	}
	socketServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // ͨ��Э�飺IPv4 InternetЭ��; �׽���ͨ�����ͣ�TCP����;  Э���ض����ͣ�ĳЩЭ��ֻ��һ�����ͣ���Ϊ0

	u_long mode = 1;
	ioctlsocket(socketServer, FIONBIO, &mode); // ��������ģʽ���ȴ����Կͻ��˵����ݰ�

	socketAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);  // 0.0.0.0, ����������IP
	socketAddr.sin_family = AF_INET;  // ʹ��IPv4��ַ
	socketAddr.sin_port = htons(SERVER_PORT);  // �˿ں�


	err = bind(socketServer, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));  // �󶨵�socketServer
	if (err == SOCKET_ERROR) {
		err = GetLastError();
		cout << "Could not bind the port " << SERVER_PORT << " for socket. Error message: " << err << endl;
		WSACleanup();
		return;
	}
	else {
		printTime();
		cout << "�����������ɹ����ȴ��ͻ��˽�������" << endl;
	}
}


void sendSYNACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setSYNACK();
	sendto(socketServer, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "���ʹ�ACK��SYN�����ݰ�..." << endl;
}

bool receiveSYN(Packet* recvPkt) {

	// �����׽���Ϊ����ģʽ�������������Ϊ
	unsigned long mode = 0;
	ioctlsocket(socketServer, FIONBIO, &mode);  // ����Ϊ����ģʽ

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0 && (recvPkt->FLAG & 0x2)) {
		printTime();
		cout << "�յ��ͻ��˵�SYN���ݰ���׼���ڶ�������..." << endl;
		return true;
	}
	return false;
}

bool receiveACK(Packet* recvPkt) {

	// �����׽���Ϊ����ģʽ�������������Ϊ
	unsigned long mode = 0;
	ioctlsocket(socketServer, FIONBIO, &mode);  // ����Ϊ����ģʽ

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0 && (recvPkt->FLAG & 0x4)) {
		printTime();
		cout << "�յ��ͻ��˵�ACKȷ�����ݰ�..." << endl;
		return true;
	}
	return false;
}

void connect()
{
	int state = 0;  // ��ʶĿǰ���ֵ�״̬
	bool connectionEstablished = 0;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;
	while (!connectionEstablished)
	{
		// ��һ������: �ȴ��ͻ��˷���SYN
		if (receiveSYN(recvPkt)) {
			sendSYNACK();  // �ڶ�������: ����SYN-ACK
			// ����������: �ȴ��ͻ���ȷ��ACK
			if (receiveACK(recvPkt)) {
				connectionEstablished = 1;
			}
		}
	}
	printTime();
	cout << "����������ɣ������ѽ�������ʼ�ļ�����..." << endl;
	cout << "----------------------------------------------------------------------" << endl;

}
bool receiveFIN(Packet* recvPkt) {

	// �����׽���Ϊ����ģʽ�������������Ϊ
	unsigned long mode = 0;
	ioctlsocket(socketServer, FIONBIO, &mode);  // ����Ϊ����ģʽ

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0 && (recvPkt->FLAG & 0x1)) {
		printTime();
		cout << "�յ��ͻ��˵�FIN���ݰ�..." << endl;
		return true;
	}
	return false;
}
void sendACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setACK();
	sendto(socketServer, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "���ʹ�ACK�����ݰ�..." << endl;
}
void sendFINACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setFINACK();
	sendto(socketServer, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "���ʹ�ACK��FIN�����ݰ�..." << endl;
}
void disconnect() {
	int state = 0;  // ��ʶĿǰ���ֵ�״̬
	bool flag = 1;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;
	receiveFIN(recvPkt);//��һ�λ��֣�����FIN
	sendACK();  // �ڶ��λ���: ����ACK
	sendFINACK();// �����λ���: ����ACK��FIN
	receiveACK(recvPkt);//��������
}
void saveFile() {
	string filePath = "D:/28301/Desktop/get/";  // ����·��
	for (int i = 0; fileName[i]; i++)filePath += fileName[i];
	ofstream fout(filePath, ios::binary | ios::out);

	fout.write(fileBuffer, fileSize); // ���ﻹ��size,���ʹ��string.data��c_str�Ļ�ͼƬ����ʾ�������������
	fout.close();
}

void receiveHead(Packet* recvPkt, Packet* sendPkt)
{
	// �����׽���Ϊ����ģʽ�������������Ϊ
	unsigned long mode = 0;
	ioctlsocket(socketServer, FIONBIO, &mode);  // ����Ϊ����ģʽ

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0) {
		printReceivePacketMessage(sendPkt);
		if (isCorrupt(recvPkt)) {  // �������ݰ���
			printTime();
			cout << "�յ�һ���𻵵����ݰ������Ͷ˷��� ack=" << waitSeq << " ���ݰ�" << endl;

			sendPkt->ack = waitSeq - 1;
			sendPkt->setACK();
			sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
		}
		//���շ��յ����� HEAD ��־λ�����ݰ��󣬻�������е��ļ������ļ���С��Ϣ��Ϊ�������ļ���������׼����
		//���շ�������㹻�Ļ��������洢���յ����ļ����ݣ���׼���ý��պ��������ݰ���
		if (recvPkt->FLAG & 0x8) {  // HEAD=1
			fileSize = recvPkt->len;
			fileBuffer = new char[fileSize];
			fileName = new char[128];
			memcpy(fileName, recvPkt->data, strlen(recvPkt->data) + 1);

			//��� fileSize % PACKET_LENGTH ��Ϊ 0��˵���ļ���С�������ݰ���С���������������Ҫ�෢��һ�����ݰ�������ʣ������ݡ�
			//��� fileSize% PACKET_LENGTH Ϊ 0��˵���ļ���С���������ݰ���С��������������Ҫ��������ݰ���
			packetNum = fileSize % PACKET_LENGTH ? fileSize / PACKET_LENGTH + 1 : fileSize / PACKET_LENGTH;

			// ��Ԫ����� ? : ��һ����������������ڸ�������ѡ���������ʽ�е�һ����
			// �﷨��condition ? expression1 : expression2
			//		��� condition Ϊ�棨���㣩�����������ʽ��ֵΪ expression1��
			//		��� condition Ϊ�٣��㣩�����������ʽ��ֵΪ expression2��


			printTime();
			cout << "�յ����Է��Ͷ˵��ļ�ͷ���ݰ����ļ���Ϊ: " << fileName;
			cout << "���ļ���СΪ: " << fileSize << " Bytes���ܹ���Ҫ���� " << packetNum << " �����ݰ�";
			cout << "���ȴ������ļ����ݰ�..." << endl;

			waitSeq++;

			sendPkt->ack = waitSeq - 1;
			sendPkt->setACK();
			sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));

		}
		else {
			printTime();
			cout << "�յ������ݰ������ļ�ͷ���ȴ����Ͷ��ش�..." << endl;
		}
	}
}
void receiveData(Packet* recvPkt, Packet* sendPkt)
{
	// �����׽���Ϊ����ģʽ�������������Ϊ
	unsigned long mode = 0;
	ioctlsocket(socketServer, FIONBIO, &mode);  // ����Ϊ����ģʽ

	int err = recvfrom(socketServer, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0) {
		printReceivePacketMessage(recvPkt);
		if (isCorrupt(recvPkt)) {  // �������ݰ���
			printTime();
			// ��ȡ��׼������
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			// �����ı���ɫΪ��ɫ
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
			cout << "���������ݰ������Ͷ˷��� ack=" << waitSeq << " ���ݰ�" << endl;
			// �ָ�Ĭ���ı���ɫ
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

			sendPkt->ack = waitSeq - 1;
			sendPkt->setACK();
			sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
		}
		if (recvPkt->seq == waitSeq) {  // �յ���Ŀǰ�ȴ��İ�
			if (randomNumber(randomEngine) >= 0.01) {
				memcpy(fileBuffer + recvSize, recvPkt->data, recvPkt->len);
				recvSize += recvPkt->len;

				printTime();
				cout << "�յ��� " << recvPkt->seq << " �����ݰ������Ͷ˷��� ack=" << waitSeq << endl;

				waitSeq++;

				sendPkt->ack = waitSeq - 1;
				sendPkt->setACK();
				sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
			}
			else {

				if (randomNumber(randomEngine) >= 0.5)
				{
					// ��ȡ��׼������
					HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
					// �����ı���ɫΪ��ɫ
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
					cout << "��������" << endl;
					// �ָ�Ĭ���ı���ɫ
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
				}
				else
				{
					printTime();
					// ��ȡ��׼������
					HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
					// �����ı���ɫΪ��ɫ
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
					cout << "���������ݰ������Ͷ˷��� ack=" << waitSeq << " ���ݰ�" << endl;
					// �ָ�Ĭ���ı���ɫ
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);


					sendPkt->ack = waitSeq - 1;
					sendPkt->setACK();
					sendto(socketServer, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
				}
			}
		}
		else {  // ����Ŀǰ�ȴ������ݰ�
			printTime();
			cout << "�յ��� " << recvPkt->seq << " �����ݰ�������������Ҫ�����ݰ������Ͷ˷��� ack=" << waitSeq << endl;
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
	cout << "�ļ��������..." << endl;
	cout << "----------------------------------------------------------------------" << endl;

	// �����ļ�
	saveFile();

	flag = 0;
	disconnect();

}

int main() {
	// ��ʼ��������socket
	initSocket();

	// ��������
	connect();

	// ��ʼ�����ļ�
	receiveFile();

	// ������ɺ󣬹ر�socket
	err = closesocket(socketServer);
	if (err == SOCKET_ERROR) {
		cout << "Close socket failed with error: " << WSAGetLastError() << endl;
		return 1;
	}

	// �����˳�
	printTime();
	cout << "�����˳�..." << endl;
	WSACleanup();

	system("pause");
	return 0;
}