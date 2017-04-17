//this program receives messages/files from a sender over udp protocol. In this program I will implement
//reliable data transfer on top of UDP using checksums, sequence nums, and ACK's
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sstream>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>

using namespace std;

struct DatagramSendSocket{//used sender-receiver bc didnt want to deal with binding/unbinding socket

	int socketFd;
	struct addrinfo *receiverInfo;

	DatagramSendSocket(const char* host, const char* port);//sets up a send socket
	void sendAck(string currPacket);


};

struct DatagramRecvSocket{

	int socketFd;
	struct addrinfo *receiverInfo;//sets up a receive socket
	char recvBuffer[512];

	DatagramRecvSocket(const char *port);
	string receivePacket();

};

//helpers
void log(string message);       //log message to stdout
//utility methods
signed char getCheckSum(string content);
void writePacketToFile(int currPacket,int totalBytes, string data);

int main(int argc, char *argv[] ) {

	log("starting receiver...");

	if(argc!=4){
		cout<<"Incorrect args.\n USAGE: recvPort senderLocation senderPort\n";
	}

	DatagramRecvSocket* recv = new DatagramRecvSocket(argv[2]);        //initialize communication sockets
	DatagramSendSocket* send = new DatagramSendSocket(argv[3],argv[4]);

	while(true){

		log("listening for a packet...");
		string packet = recv->receivePacket();   //rcv from sender

		string header = packet.substr(0,35);
		string data = packet.substr(25,string::npos); //segment packet
		log("packet received data size = "+to_string(data.size()));
		log("HEADER: "+header);

		if(header.at(33)==getCheckSum(data)) {//if checksum is valid

			send->sendAck(header.substr(9, 15)); //ping back

			if (header.at(7) == 'F') {//if file write

				log("packet has command type F");
				writePacketToFile(atoi(header.substr(9, 15)), atoi(header.substr(23, 32)), data);

			} else if (header.at(7) == 'M') {//if message print

				log("packet has command type M");
				log("printing message...");
				cout << data << "\n";

			}else{//if checksum not valid discard and carry on
				log("Invalid checksum. Data curruption detected. Discarding packet...");
			}
		}

	}

}

//writes file data to file regardless if in sequence. Each file
void writePacketToFile(int currPacket,int totalBytes, string data){

	int index = data.find(":");
	string fileName = data.substr(index); //segment packet file data
	data = data.substr(index+1,string::npos);

	FILE * receiveFile;
	if((receiveFile = fopen(fileName.c_str(), "w"))==NULL){
		perror("file open failed for get file");
	}

	fseek(receiveFile,0,SEEK_END);
	int size = ftell(receiveFile);
	rewind(receiveFile);
	log("size of file is "+to_string(size)+"bytes");

	if(size<totalBytes){

		log("adjusting file to correct length of "+to_string(totalBytes)+"bytes...");
		if(ftruncate(fileno(receiveFile),totalBytes)==-1){
			perror("truncate");
		}

	}

	log("adjusting file cursor to correct place for write...");
	fseek(receiveFile,477*(currPacket-1),SEEK_SET);

	log("writing chunk "+to_string(currPacket)+" to "+fileName+"...");
	if(fwrite(data.c_str(), sizeof(char), data.length(),receiveFile)==-1){
		perror("write");
	}
}

signed char getChecksum(string content){

	char * c = (char *) content.c_str();
	signed char checkSum = -1;

	while(*c != 0){

		checkSum += *c;
		c++;

	}

	log("checksum for data chunk calculated to be "+checkSum);
	return checkSum;
}

string DatagramRecvSocket::receivePacket() {

	if(recv(socketFd, recvBuffer, 512, 0)==-1){
		perror("recv");
	}

	string data(recvBuffer);
	return data;
}

void DatagramRecvSocket::DatagramRecvSocket(const char *port) {//sender's host and port

	log("creating recv socket...");
	struct addrinfo prepInfo;

	memset(&prepInfo, 0, sizeof prepInfo);
	prepInfo.ai_family = AF_UNSPEC;
	prepInfo.ai_socktype = SOCK_DGRAM;
	prepInfo.ai_protocol = IPPROTO_UDP;

	int code;
	if((code = getaddrinfo("127.0.0.1",port,&prepInfo,&receiverInfo))!=0){
		cout<<gai_strerror(code)<<'\n';
	}

	if((socketFd = socket( receiverInfo->ai_family, receiverInfo->ai_socktype, receiverInfo->ai_protocol))==-1){
		perror("socket setup");
	}

	if(bind(socketFd,receiverInfo->ai_addr,receiverInfo->ai_addrlen)!=0){
		perror("bind");
	}

}

void DatagramSendSocket::sendAck(string currPacket) {

	log("sending ACK:"+currPacket+"...");
	string ackStr = "ACK:+"+currPacket;
	if(sendto(socketFd, ackStr.c_str(), 512, 0, receiverInfo->ai_addr, receiverInfo->ai_addrlen)==-1){
		perror("send");
	}

}

DatagramSendSocket::DatagramSendSocket(const char *host, const char *port){//receiver's host and port

	log("creating send socket...");
	struct addrinfo prepInfo;

	memset(&prepInfo, 0, sizeof prepInfo);
	prepInfo.ai_family = AF_UNSPEC;
	prepInfo.ai_socktype = SOCK_DGRAM;
	prepInfo.ai_protocol = IPPROTO_UDP;

	int code;
	if((code = getaddrinfo(host,port,&prepInfo,&receiverInfo))!=0){
		cout<<gai_strerror(code)<<'\n';

	}

	if((socketFd = socket( receiverInfo->ai_family, receiverInfo->ai_socktype, receiverInfo->ai_protocol))==-1){
		perror("socket setup");
	}

}

void log(string message){
	cout << "LOG: " << message << '\n';
}
