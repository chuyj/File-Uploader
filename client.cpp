#include <time.h>
#include <string>
#include <netdb.h>
#include <cstdlib>
#include <fcntl.h>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/select.h>
#include <netinet/in.h>
#include "myheader.h"
using namespace std;
string to_str(int num) {
	stringstream ss;
	ss << num;
	string ret;
	ss >> ret;
	return ret;
}
int sockfd;
string MYDIR;
fd_set r_set, all_set;
int getFsize(int fd) {
	struct stat buf;
	fstat(fd, &buf);
	return buf.st_size;
}
void showbar(int cur, int all) {
	int rate = (cur * 20 / all);
	cout << "\rProgress : [";
	for (int i = 0; i < 20; ++i) {
		if (i < rate) cout << "#";
		else cout << " ";
	}
	cout << "]" << rate * 5 << "%" << flush;
}
void sendfile(string fname) {
	int fd = open(fname.c_str(), O_RDONLY);	
	if (fd <0) {
		cout << "File open error" << endl;
		return ;
	} 
	cout << "Uploading file : " << fname << endl;
	mywrite(sockfd, FNAME, fname.size(), fname.c_str());
	int fsize = getFsize(fd);

	mywrite(sockfd, FSIZE, fsize, 0);
	int n, sent = 0;
	char buf[MAXLINE];
	while ((n = read(fd, buf, MAXLINE)) > 0) {
		mywrite(sockfd, FDATA, n, buf);	
		showbar(sent, fsize);
		sent += n;
	}	
	mywrite(sockfd, MYEOF, 0, 0);
	showbar(sent, fsize);
	cout << "\nUpload " << fname << " complete!" << endl;
}
void readfile() {
	Msg msg = myread(sockfd);
	if (msg.mod == CONNCLOSE) {
		cout << "Connection closed by server" << endl;
		FD_CLR(sockfd, &all_set);
		exit(EXIT_FAILURE);
	}
	//read name
	if (msg.mod != FNAME) {
		cout << "[Error] Should receive file name" << endl;
		return ;
	}
	msg.buf[msg.len] = 0;	
	string fname = msg.buf;
	int fd = creat((MYDIR + "/" + fname).c_str() ,0664);
	//read size
	msg = myread(sockfd);
	if (msg.mod != FSIZE) {
		cout << "[Error] Should receive file size" << endl;
		return ;
	}
	int fsize = msg.len;
	int received = 0;
	//read data
	cout << "Downloading file : " + fname << endl;
	while (true) {
		msg = myread(sockfd);
		if (msg.mod == MYEOF) break;
		if (msg.mod != FDATA) {
			cout << "[Error] Should receive data" << endl;
			return ;
		}	
		write(fd, msg.buf, msg.len);
		msg.buf[msg.len] = 0;
		received += msg.len;
		showbar(received,fsize);
	}
	cout << endl <<  "Download "+ fname + " complete!" << endl;
	return ;
}
void mysleep(int time) {
	cout << "Client starts to sleep" << endl;
	for (int i = 1; i <= time; ++i) {
		cout << "Sleep " << i << endl;
		sleep(1);
	}	
	cout << "Client wakes up" << endl;
}
int main(int argc, char** argv) {
	if (argc < 4) {
		cout << "Usage: " + string(argv[0]) + " <ip> <port> <username>" << endl;
		exit(EXIT_FAILURE);
	}
	struct hostent * hp = gethostbyname(argv[1]);
	if (hp == NULL) {
		cout << "[Error] Get hostbyname error" << endl;
		exit(EXIT_FAILURE);
	}
	if (hp->h_addr_list[0] == NULL) cout << "[Error] Get host error" << endl;
	char * host = inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0]));
	cout << host << endl;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cout << "Socket create failure" << endl;
		exit(EXIT_FAILURE);
	}

	sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, host, &servaddr.sin_addr);
	if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		cout << "Connection Error" << endl;
		exit(EXIT_FAILURE);
	}


	FD_ZERO(&r_set);
	FD_ZERO(&all_set);
	FD_SET(sockfd, &all_set);
	FD_SET(STDIN_FILENO, &all_set);
	int maxfd = max(sockfd, STDIN_FILENO);
	
	int flag = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, 2);
	fcntl(STDOUT_FILENO, F_SETFL, 2);

	char buf[MAXLINE];
	//send user name
	string name = argv[3];
	mywrite(sockfd, UNAME, name.size(), name.c_str());

	srand(time(NULL));
	MYDIR = CLIDIR + to_str(rand()%1000);
	system(("mkdir " + MYDIR).c_str());
	cout << "Welcome to the dropbox-like server! : " << argv[3] << " (" << MYDIR << ")" << endl;
	

	while (true) {
		r_set = all_set;
		select(maxfd+1, &r_set, NULL, NULL, NULL);
		if (FD_ISSET(STDIN_FILENO, &r_set)) {
			int n = read(STDIN_FILENO, buf, MAXLINE);
			buf[n] = 0;	
			stringstream ss(buf);
			string cmd; ss >> cmd;
			if (cmd == "/put") {
				string fname; 
				if(!(ss >> fname)) {
					cout << "Usage: /put <filename>" << endl;
				} else
					sendfile(fname);
			} else if (cmd == "/sleep") {
				int time;
				if (!(ss >> time))
					cout << "Usage: /sleep <time>" << endl;
				else
					mysleep(time);
			} else if (cmd == "/exit") {
				break;
			} else {
				cout << "[Error] Undefined Command: " << cmd << endl;
				cout << "Commands:" << endl;
				cout << "\t/put <filename>" << endl;
				cout << "\t/sleep <time>" << endl;
				cout << "\t/exit" << endl;
			}
		}
		if (FD_ISSET(sockfd, &r_set)) {
			readfile();	
		}
	}
	return 0;
}
