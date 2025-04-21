#include "Client.h"

#define SERVER_IP "127.0.0.1"  // ��������IP��ַ
#define SERVER_PORT 9528	// �˿�
#define BUFFER_SIZE sizeof(Packet)  // ��������С
#define WINDOW_SIZE 20
#define PACKET_LENGTH 10240
#define MAX_TIME 2 * CLOCKS_PER_SEC  // ��ʱ�ش�

#include <windows.h>

SOCKADDR_IN socketAddr;  // ��������ַ
SOCKET socketClient;  // �ͻ����׽���

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
	SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);

	// ��ӡʱ��
	cout << ss.str();

	// �ָ�Ĭ���ı���ɫ
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);


}

void printPacketMessage(Packet* pkt) {  // ��ӡ���ݰ���Ϣ 

	// ��ȡ��׼������
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// �����ı���ɫΪ��ɫ
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

	cout << "Packet size=" << pkt->len << " Bytes  FLAG=" << pkt->FLAG;
	cout << " seqNumber=" << pkt->seq << " ackNumber=" << pkt->ack;
	cout << " checksum=" << pkt->checksum << " windowLength=" << pkt->window << endl;

	// �ָ�Ĭ���ı���ɫ
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void printSendPacketMessage(Packet* pkt) {

	cout << "UDP_Packet_info��";
	printPacketMessage(pkt);
}

int fromLen = sizeof(SOCKADDR);
int packetNum;  // �������ݰ�������
int fileSize;  // �ļ���С
int sendSeq;  // �������ݰ����к�
int err;  // socket������ʾ
char* filePath;
char* fileName;
char* fileBuffer;

void initSocket() {
	WSADATA wsaData;
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);  // �汾 2.2 
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
	cout << "�ͻ��˳�ʼ���ɹ���׼�����������������..." << endl;
}

void sendSYNACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setSYNACK();
	sendto(socketClient, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "���ʹ�ACK��SYN�����ݰ�..." << endl;
}

bool receiveSYN(Packet* recvPkt) {

	// �����׽���Ϊ����ģʽ�������������Ϊ
	unsigned long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);  // ����Ϊ����ģʽ

	int err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
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
	ioctlsocket(socketClient, FIONBIO, &mode);  // ����Ϊ����ģʽ

	int err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err > 0 && (recvPkt->FLAG & 0x4)) {
		printTime();
		cout << "�յ��ͻ��˵�ACKȷ�����ݰ�..." << endl;
		return true;
	}
	return false;
}

bool receiveFIN(Packet* recvPkt) {

	// �����׽���Ϊ����ģʽ�������������Ϊ
	unsigned long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);  // ����Ϊ����ģʽ

	int err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
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
	sendto(socketClient, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "���ʹ�ACK�����ݰ�..." << endl;
}
void sendFINACK() {
	Packet* pktToSend = new Packet;
	pktToSend->setFINACK();
	sendto(socketClient, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "���ʹ�ACK��FIN�����ݰ�..." << endl;
}


void sendSYN(Packet* pktToSend)
{
	pktToSend->setSYN();
	sendto(socketClient, (char*)pktToSend, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), sizeof(SOCKADDR));
	printTime();
	cout << "���ʹ�SYN�����ݰ�..." << endl;
}

void receiveSYNACK(Packet* recvPkt, Packet* sendPkt) {

	// �����׽���Ϊ����ģʽ�������������Ϊ
	unsigned long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);  // ����Ϊ����ģʽ

	int err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if ((recvPkt->FLAG & 0x2) && (recvPkt->FLAG & 0x4)) {  // SYN=1, ACK=1
		printTime();
		cout << "��ʼ���������֣������������ACK=1�����ݰ�..." << endl;

		// ���������֣������������ACK=1�����ݰ�����֪�������Լ�������������
		sendPkt->setACK();
		sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));

		printTime();
		cout << "�������ֽ�����ȷ���ѽ������ӣ���ʼ�ļ�����..." << endl;
		cout << "----------------------------------------------------------------------" << endl;
	}
	else {
		printTime();
		cout << "�ڶ������ֽ׶��յ������ݰ���������ʧ��..." << endl;
	}
}
void connect() {
	int state = 0;  // ��ʶĿǰ���ֵ�״̬
	bool flag = 1;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;

	sendSYN(sendPkt);// ��һ�����֣������������SYN=1�����ݰ�
	receiveSYNACK(recvPkt, sendPkt);//

}

void sendFIN(Packet* sendPkt)
{
	printTime();
	cout << "��ʼ��һ�λ��֣������������FIN=1�����ݰ�..." << endl;

	// ��һ�λ��֣������������FIN=1�����ݰ�
	sendPkt->setFIN();
	sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
}
void receiveFINACK(Packet* recvPkt, Packet* sendPkt)
{
	// �����׽���Ϊ����ģʽ�������������Ϊ
	unsigned long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);  // ����Ϊ����ģʽ

	err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen);
	if (err >= 0) {
		if ((recvPkt->FLAG & 0x1) && (recvPkt->FLAG & 0x4)) {  // ACK=1, FIN=1
			printTime();
			cout << "�յ������Է����������λ���FIN&ACK���ݰ�����ʼ���Ĵλ��֣������������ACK=1�����ݰ�..." << endl;

			// ���Ĵλ��֣������������ACK=1�����ݰ���֪ͨ������ȷ�϶Ͽ�����
			sendPkt->setFINACK();
			sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));

			printTime();
			cout << "�Ĵλ��ֽ�����ȷ���ѶϿ�����..." << endl;
		}
		else {
			printTime();
			cout << "�����λ��ֽ׶��յ������ݰ��������¿�ʼ�����λ���..." << endl;
		}
	}
}
void disconnect() {
	int state = 0;  // ��ʶĿǰ���ֵ�״̬	
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

	// ��ȡ��׼������
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// �����ı���ɫΪ��ɫ
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED);

	cout << "��ʱ�������·��ʹ����ݰ�" << endl;

	// �ָ�Ĭ���ı���ɫ
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}
void sendData(Packet* sendPkt)
{
	err = sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
	if (err == SOCKET_ERROR) {

		// ��ȡ��׼������
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

		// �����ı���ɫΪ��ɫ
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED);

		cout << "sendto failed with error: " << WSAGetLastError() << endl;

		// �ָ�Ĭ���ı���ɫ
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	}
}
void waitACK(Packet* recvPkt, Packet* sendPkt)
{
	clock_t start = clock();  // ��¼����ʱ�䣬��ʱ�ش�

	// �ȴ�����ACK��Ϣ����֤acknowledge number
	while (true) {
		// �����׽���Ϊ������ģʽ
		unsigned long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);  // ����Ϊ������ģʽ

		while (recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &fromLen) <= 0) {
			if (clock() - start > MAX_TIME) {
				printOverTime();
				sendData(sendPkt);
				start = clock(); // ���迪ʼʱ��
			}
		}
		// ��������Ҫͬʱ���㣬�����ж���packet��seq
		if ((recvPkt->FLAG & 0x4) && (recvPkt->ack == sendPkt->seq)) {
			sendSeq++;
			break;
		}
		else
		{
			printTime();

			// ��ȡ��׼������
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

			// �����ı���ɫΪ��ɫ
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED);

			cout << "���ݰ��𻵣������ط�" << endl;

			// �ָ�Ĭ���ı���ɫ
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

			sendData(sendPkt);
			start = clock(); // ���迪ʼʱ��
		}
	}
}
void sendPacket(Packet* sendPkt) {
	Packet* recvPkt = new Packet;

	// ���һ���ļ��ĸ�������
	printSendPacketMessage(sendPkt);
	sendData(sendPkt);

	waitACK(recvPkt, sendPkt);
}






void readFile() {
	ifstream f(filePath, ifstream::in | ios::binary);  // �Զ����Ʒ�ʽ���ļ�
	if (!f.is_open()) {
		// ��ȡ��׼������
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

		// �����ı���ɫΪ��ɫ
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED);

		cout << "�ļ��޷��򿪣�" << endl;

		// �ָ�Ĭ���ı���ɫ
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		return;
	}
	f.seekg(0, std::ios_base::end);  // ���ļ���ָ�붨λ������ĩβ
	fileSize = f.tellg();
	packetNum = fileSize % PACKET_LENGTH ? fileSize / PACKET_LENGTH + 1 : fileSize / PACKET_LENGTH;
	cout << "�ļ���СΪ " << fileSize << " Bytes���ܹ�Ҫ���� " << packetNum + 1 << " �����ݰ�" << endl;
	f.seekg(0, std::ios_base::beg);  // ���ļ���ָ�����¶�λ�����Ŀ�ʼ
	fileBuffer = new char[fileSize];
	f.read(fileBuffer, fileSize);
	f.close();
}

void sendFile() {
	sendSeq = 0;
	clock_t start = clock();

	// �ȷ�һ����¼�ļ��������ݰ���������HEAD��־λΪ1����ʾ��ʼ�ļ�����
	// HEAD ��־λ���ڱ�ʶһ�����ݰ����ļ������ͷ����Ϣ��
	// ���ļ����俪ʼʱ�����ͷ��ᷢ��һ������ HEAD ��־λ�����ݰ���������ݰ��������ļ��Ļ�����Ϣ�����ļ������ļ���С��
	Packet* headPkt = new Packet;
	printTime();
	cout << "�����ļ�ͷ���ݰ�..." << endl;
	headPkt->setHEAD(sendSeq, fileSize, fileName);
	headPkt->checksum = checksum((uint32_t*)headPkt);
	sendPacket(headPkt);

	// ��ʼ����װ���ļ������ݰ�
	printTime();
	cout << "��ʼ�����ļ����ݰ�..." << endl;
	Packet* sendPkt = new Packet;
	for (int i = 0; i < packetNum; i++) {
		if (i == packetNum - 1) {  // ���һ����
			sendPkt->setTAIL();
			sendPkt->fillData(sendSeq, fileSize - i * PACKET_LENGTH, fileBuffer + i * PACKET_LENGTH);
			sendPkt->checksum = checksum((uint32_t*)sendPkt);
			//TAIL ��־λ���ڱ�ʶ�ļ�����Ľ��������ͷ�ͨ������֪���շ��ļ���������ɣ����շ�����ֹͣ�ȴ��µ����ݰ������к�������
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
	cout << "�ļ��������..." << endl;
	cout << "����ʱ��Ϊ��" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
	cout << "������Ϊ��" << ((float)fileSize) / ((end - start) / CLOCKS_PER_SEC) << " Bytes/s " << endl;
	cout << "----------------------------------------------------------------------" << endl;
}

int main() {
	// ��ʼ���ͻ���
	initSocket();

	// ��������˽�������
	connect();

	cout << "��������Ҫ������ļ�����·��(����·��)��";
	filePath = new char[128];
	cin >> filePath;

	// �� char* ת��Ϊ string�����ڲ���
	string path(filePath);

	// �ҵ�·�������һ����б�ܵ�λ��
	size_t pos = path.find_last_of("\\/");  // ֧�ַ�б�ܺ���б��

	// ��ȡ�ļ������֣���б��֮��Ĳ��֣�
	string fileNameStr = path.substr(pos + 1);  // ��ȡ�ļ���

	// ����ȡ�����ļ������Ƶ�ȫ�ֱ��� fileName
	fileName = new char[fileNameStr.length() + 1]; // +1 for null terminator
	strcpy(fileName, fileNameStr.c_str());  // ʹ�� strcpy ���ַ������Ƶ� fileName

	readFile();

	// ��ʼ�����ļ�
	sendFile();

	// �����Ͽ�����
	disconnect();

	// ������ɺ󣬹ر�socket
	err = closesocket(socketClient);
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