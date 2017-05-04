#ifndef __MYHEADER__
#define __MYHEADER__
#include <string>

#include <sstream>
#include <iostream>
#include <arpa/inet.h>
using namespace std;
const int MAXLINE = 256;
const int UNAME = MAXLINE+1;
const int FNAME = MAXLINE+2;
const int FSIZE = MAXLINE+3;
const int FDATA = MAXLINE+4;
const int MYEOF = MAXLINE+5;
const int FEND = MAXLINE+6;
const int CONNCLOSE = MAXLINE+7;


const string SERVDIR = "Server";
const string CLIDIR = "Client";
int w_count = 0;
int r_count = 0;

std::string getMod(int mod) {
	switch(mod) {
		case(UNAME): return "UNAME";
		case(FNAME): return "FNAME";
		case(FSIZE): return "FSIZE";
		case(FDATA): return "FDATA";
		case(MYEOF): return "MYEOF";
		case(CONNCLOSE): return "CONNCLOSE";
		default: return "UNDEFINED";
	}
}
#pragma pack(1)
struct Msg {
	Msg(const int& mod_, const int& len_, const char* buf_) {
		if (mod_ == FDATA || mod_ == FNAME || mod_ == UNAME) {
			for (int i = 0; i < MAXLINE && i < len_; ++i) {
				buf[i] = buf_[i];
			}
		}
		mod = htonl(mod_);
		len = htonl(len_);
	}
	Msg(){}
	int mod;
	int len;
	char buf[MAXLINE+1];
};
#pragma pack()
int mywrite(const int& sockfd, const int& mod, const int& len, const char* buf) {
	Msg msg(mod, len, buf);
	return write(sockfd, (char*)&msg, sizeof(msg));
}
Msg myread(const int&sockfd) {
	Msg msg;
	int got = 0;
	bool connclose = false;
	while (got < sizeof(msg)) {
		int n = read(sockfd, ((char*)&msg)+got , sizeof(msg)-got);
		if (n == 0) {
			connclose = true;
			break;
		} else if (n < 0) {
			if (errno == EWOULDBLOCK) {
				continue;
			} else {
				cout << "[Error] Really bad" << endl; 
			}
		} else {
			got += n;
		}
	}
	msg.mod = ntohl(msg.mod);
	msg.len = ntohl(msg.len);
	if (connclose == true) msg.mod = CONNCLOSE;
	if (msg.mod == FSIZE || msg.mod == CONNCLOSE)
		msg.buf[0] = 0;
	else {
		if (msg.len > 256) 
		msg.buf[msg.len] = 0;
	}
	return msg;
}
#endif
