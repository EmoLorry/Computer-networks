#pragma once
#ifndef __PKT_H__
#define __PKT_H__

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)
//禁用编译器警告4996。

#pragma comment(lib,"ws2_32.lib") 
#include <stdlib.h> 
#include <time.h> 
#include <WinSock2.h> 
#include <fstream> 
#include <iostream>
#include <cstdint>
#include <vector>
#include <sstream>
#include <random>
using namespace std;

#endif
//结束条件编译块，与 #ifndef __PKT_H__ 对应。

#ifndef __DEFINE_H__
#define __DEFINE_H__


class Packet {
public:
	uint32_t FLAG;  // 标志位，用于表示数据包的类型，目前只用最后五位（二进制）分别是TAIL, HEAD, ACK，SYN，FIN
	uint32_t seq;  // 序列号，用于标识数据包的顺序，uint32_t相当于unsigned int，只不过是不管是多少位的系统他都是32位
	uint32_t ack;  // 确认号，用于确认收到的数据包
	uint32_t len;  // 数据部分长度
	uint32_t checksum;  // 校验和
	uint32_t window;  // 窗口
	char data[10240];  // 数据长度
public:
	Packet() : FLAG(0), seq(0), ack(0), len(0), checksum(0), window(0) { memset(data, 0, sizeof(data)); };
	Packet(uint32_t FLAG, uint32_t seq, uint32_t ack, uint32_t len, uint32_t checksum, uint32_t window);
	void setACK();
	void setSYN();
	void setSYNACK();
	void setFIN();
	void setFINACK();
	void setTAIL();
	void setHEAD(int seq, int fileSize, char* fileName);
	void fillData(int seq, int size, char* data);
};

Packet::Packet(uint32_t FLAG, uint32_t seq, uint32_t ack, uint32_t len, uint32_t checksum, uint32_t window) {
	this->FLAG = FLAG;
	this->seq = seq;
	this->ack = ack;
	this->len = len;
	this->checksum = checksum;
	this->window = window;
	memset(this->data, 0, sizeof(this->data));
}

void Packet::setTAIL() {
	// 设置TAIL位为1
	this->FLAG = 0x16;  // FLAG -> 00010000
}

void Packet::setHEAD(int seq, int fileSize, char* fileName) {
	// 设置HEAD位为1    
	this->FLAG = 0x8;  // FLAG -> 00001000

	// 这里的len并不是data的len，而是整个文件的size
	this->len = fileSize;
	this->seq = seq;
	memcpy(this->data, fileName, strlen(fileName) + 1);
}

void Packet::setACK() {
	// 设置ACK位为1
	this->FLAG = 0x4;  // FLAG -> 00000100
}

void Packet::setSYN() {
	// 设置SYN位为1
	this->FLAG = 0x2;  // FLAG -> 00000010
}

void Packet::setSYNACK() {
	// 设置SYN, ACK = 1
	this->FLAG = 0x4 + 0x2;  // FLAG -> 00000110
}

void Packet::setFIN() {
	// 设置FIN位为1
	this->FLAG = 0x1;  // FLAG -> 00000001
}

void Packet::setFINACK() {
	// 设置FIN, ACK = 1    
	this->FLAG = 0x4 + 0x1;  // FLAG -> 00000101
}

void Packet::fillData(int seq, int size, char* data) {
	// 将文件数据填入数据包data变量
	this->seq = seq;
	this->len = size;
	memcpy(this->data, data, size);
}

// 计算数据包的校验和。校验和用于检测数据包在传输过程中是否发生错误。
uint16_t checksum(uint32_t* pkt) {
	int size = sizeof(pkt);
	int count = (size + 1) / 2;
	uint16_t* buf = (uint16_t*)malloc(size);  // 可以+1也可以不+1
	memset(buf, 0, size);
	memcpy(buf, pkt, size);
	u_long sum = 0;
	while (count--) {
		sum += *buf++;
		if (sum & 0xFFFF0000) {
			sum &= 0xFFFF;
			sum++;
		}
	}
	return ~(sum & 0xFFFF);
}

// 检查数据包的校验和是否匹配，如果不匹配则认为数据包损坏。
bool isCorrupt(Packet* pkt) {
	if (pkt->checksum == checksum((uint32_t*)pkt))return false;
	return true;
}
#endif
