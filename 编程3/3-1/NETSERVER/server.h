#pragma once
#ifndef __PKT_H__
#define __PKT_H__

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)
//���ñ���������4996��

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
//������������飬�� #ifndef __PKT_H__ ��Ӧ��

#ifndef __DEFINE_H__
#define __DEFINE_H__


class Packet {
public:
	uint32_t FLAG;  // ��־λ�����ڱ�ʾ���ݰ������ͣ�Ŀǰֻ�������λ�������ƣ��ֱ���TAIL, HEAD, ACK��SYN��FIN
	uint32_t seq;  // ���кţ����ڱ�ʶ���ݰ���˳��uint32_t�൱��unsigned int��ֻ�����ǲ����Ƕ���λ��ϵͳ������32λ
	uint32_t ack;  // ȷ�Ϻţ�����ȷ���յ������ݰ�
	uint32_t len;  // ���ݲ��ֳ���
	uint32_t checksum;  // У���
	uint32_t window;  // ����
	char data[10240];  // ���ݳ���
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
	// ����TAILλΪ1
	this->FLAG = 0x16;  // FLAG -> 00010000
}

void Packet::setHEAD(int seq, int fileSize, char* fileName) {
	// ����HEADλΪ1    
	this->FLAG = 0x8;  // FLAG -> 00001000

	// �����len������data��len�����������ļ���size
	this->len = fileSize;
	this->seq = seq;
	memcpy(this->data, fileName, strlen(fileName) + 1);
}

void Packet::setACK() {
	// ����ACKλΪ1
	this->FLAG = 0x4;  // FLAG -> 00000100
}

void Packet::setSYN() {
	// ����SYNλΪ1
	this->FLAG = 0x2;  // FLAG -> 00000010
}

void Packet::setSYNACK() {
	// ����SYN, ACK = 1
	this->FLAG = 0x4 + 0x2;  // FLAG -> 00000110
}

void Packet::setFIN() {
	// ����FINλΪ1
	this->FLAG = 0x1;  // FLAG -> 00000001
}

void Packet::setFINACK() {
	// ����FIN, ACK = 1    
	this->FLAG = 0x4 + 0x1;  // FLAG -> 00000101
}

void Packet::fillData(int seq, int size, char* data) {
	// ���ļ������������ݰ�data����
	this->seq = seq;
	this->len = size;
	memcpy(this->data, data, size);
}

// �������ݰ���У��͡�У������ڼ�����ݰ��ڴ���������Ƿ�������
uint16_t checksum(uint32_t* pkt) {
	int size = sizeof(pkt);
	int count = (size + 1) / 2;
	uint16_t* buf = (uint16_t*)malloc(size);  // ����+1Ҳ���Բ�+1
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

// ������ݰ���У����Ƿ�ƥ�䣬�����ƥ������Ϊ���ݰ��𻵡�
bool isCorrupt(Packet* pkt) {
	if (pkt->checksum == checksum((uint32_t*)pkt))return false;
	return true;
}
#endif
