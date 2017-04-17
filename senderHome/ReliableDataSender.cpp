//this program sends messages/files to a receiver over udp protocol. In this program I will implement
//reliable data transfer on top of UDP using checksums, a timer, sequence nums, Ack's, and neg, Ack's
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

struct DatagramSendSocket{

	int socketFd;
	struct addrinfo *receiverInfo;

	DatagramSendSocket(const char* host, const char* port);//sets up a sender socket
	void sendPacket(const char[512]);

};

struct DatagramRecvSocket{

	int socketFd;
	struct addrinfo *receiverInfo;
	char recvBuffer[512];

	DatagramRecvSocket(const char *port);
	int receiveAck(int currPacket);

};

//helpers
void log(string message);       //log message to stdout
//utility methods
string readFile(string fileName);
signed char getCheckSum(string content);

int main(int argc, char *argv[] ){

	log("starting sender");

	if(argc!=4){
		cout<<"Incorrect args.\n USAGE: senderPort recvLocation recvPort\n";
	}

	DatagramRecvSocket* recv = new DatagramRecvSocket(argv[2]);
	DatagramSendSocket* send = new DatagramSendSocket(argv[3],argv[4]);

	cout<<"Commands: -f \"filename\" or -m \"message\" to reliably send a file or message respectively. -e to exit\n";

	string input;
	while(true){

		cout<<"ReliableDataSender> ";
		getline(cin,input);

		string sendStr;
		string type;

		if(!input.substr(0,3).compare("-f ")){

			log("command type F");
			sendStr = readFile(input.substr(3));
			type = "F";

		} else
		if(!input.substr(0,3).compare("-m ")){

			log("command type M");
			sendStr = input.substr(3);
			type = "M";

		}else
		if(!input.substr(0,2).compare("-e")){

			log("command type E");
			break;

		}else{
			cout<<"invalid command. please use -m, -f, or -e\n";
			continue;
		}

		double totalPackets = ceil(sendStr.size()/477);//512 - 35
		string totalPacketsStr = to_string(totalPackets);
		log(totalPacketsStr+" packets required to send message. packet size 512(25 byte header 477 data bytes)");
		while(totalPacketsStr.length() < 6){//size standardization. wont work for packets larger than 999999*512 bytes
			totalPacketsStr = totalPacketsStr+" ";
		}
		
		int totalBytes = sendStr.size();
		string totalBytesStr = to_string(totalBytes);
		while(totalBytesStr.length()<9){//more size standardization
			totalBytesStr = " "+totalBytesStr;
		}

		for(int currPacket = 1; currPacket <= totalPackets; currPacket++){

			string chunk = sendStr.substr((currPacket-1)*477,currPacket*477);

			string currPacketStr = to_string(currPacket);
			while(currPacketStr.length()<6){//more size standardization
				currPacketStr = " "+currPacketStr;
			}

			string header = "HEADER:"+type+":"+currPacketStr+"/"+totalPacketsStr+":"+totalBytesStr+":";//35 bytes all together
			signed char checkSum = getCheckSum(chunk);//data integrity checksum
			header.push_back(checkSum);
			chunk = header+":"+chunk;

			send->sendPacket(chunk.c_str());//send data chunk
			while(recv->receiveAck(currPacket) == -1){

				send->sendPacket(chunk.c_str()); //if no ack received in 5 secs send again
			}

			log(currPacket+" packet(s) successfully sent and acknowledged");

		}//for loop

	}//main loop

	log("exiting...");

	return 0;

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

int DatagramRecvSocket::receiveAck(int currPacket) {

	int outcome = -1;
	int timeOut = 5;
	fd_set setFd;
	FD_ZERO(&setFd);
	FD_SET(socketFd,&setFd);
	struct timeval timer;
	timer.tv_sec = timeOut;//set 5 sec timeout

	log("Listening for packet with timeout of "+to_string(timeOut)+"'s...");
	int selectNum;
	if((selectNum = select(socketFd+1,&setFd,&setFd,&setFd,&timer))==-1){
		perror("select");
	} else
	if(selectNum>0){//there are fd's ready to be read

		if((outcome = recv(socketFd,recvBuffer,512,0))==-1){
			perror("recv");
		}

		char numArr[7];
		for(int i=4,j=0; i<10; i++,j++){//ack formay ACK:######
			numArr[j] = recvBuffer[i];
		}
		numArr[7]='\0';

		int ackNum = atoi(numArr);

		if(ackNum!=currPacket){
			outcome = -1;
			log("packet has improper ordering. discarding...");
		}

		log("ACK:"+to_string(ackNum)+" received. Returning ack outcome of " +to_string(outcome));
	}else{
		log("No ACK received");
	}

	return outcome;

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

void DatagramSendSocket::sendPacket(const char pckData[512]) {

	log("sending packet...");
	if(sendto(socketFd, pckData, 512, 0, receiverInfo->ai_addr, receiverInfo->ai_addrlen)==-1){
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

string readFile(string fileName){

	log("reading file named "+fileName);

	string content=fileName+":";//so recv knows name of file
	char readBuffer[512];
	FILE * file;

	if((file = fopen(fileName.c_str(), "r"))==NULL){
		perror("file open failed for "+fileName);
	}

	while(fread( readBuffer, sizeof(char), 512, file) > 0){
		string contentSeg(readBuffer);
		content+=contentSeg;
	}

	fclose(file);

	return content;
}

void log(string message){
	cout << "LOG: " << message << '\n';
}