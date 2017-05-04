#include <map>
#include <queue>
#include <string>
#include <fcntl.h>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "myheader.h"
using namespace std;

int listenfd, maxfd;
fd_set r_set, w_set, all_set;

struct File {
	File(string fname_) {
		fname = fname_;
		nextFp = 0;
		nextMsg = FNAME;
		fsize = -1;
	}
	string fname;
	int nextFp;
	int fsize;
	int nextMsg;
};
struct Client {
	void addfile(string fname) {
		fileQ.push(File(fname));
	}
	void addupfile(string fname) {
		upfile.push(File(fname));
	}
	int sockfd;
	queue<File> upfile;
	queue<File> fileQ;
	int nextMp = 0;
	bool msgQ = false;
	Msg msg;
};

map<string, vector<Client> > users;
map<string, Client> userAllFile;

void printAllFile() {
	cout << "---------------------------------" << endl;
	for (auto&& user:userAllFile) {
		cout << "User: " << user.first << endl;
		queue<File> fileQ = user.second.fileQ;
			while (!fileQ.empty()) {
				cout << "     file:" << fileQ.front().fname << " size:" << fileQ.front().fsize << endl;
				fileQ.pop();
			}
	}
	cout << "=================================" << endl;
}
void printUser() {
	cout << "---------------------------------" << endl;
	for (auto&& user:users) {
		cout << "User: " << user.first << endl;
		for (auto&& client:user.second) {
			cout << "  client:" << client.sockfd << endl;
			queue<File> fileQ = client.fileQ;
			while (!fileQ.empty()) {
				cout << "      file:" << fileQ.front().fname << " size:" << fileQ.front().fsize << endl;
				fileQ.pop();
			}
		}
	}
	cout << "=================================" << endl;
}
void addfile(string uname, string fname, int uploader) {
	for (auto&& client:users[uname]) {
		if (client.sockfd != uploader)
			client.addfile(fname);
		else
			client.addupfile(fname);
	}	
	userAllFile[uname].addfile(fname);
}
void setfsize(string uname, int fsize, int uploader) {
	for (auto&& client:users[uname]) {
		if (client.sockfd != uploader) {
			if (client.fileQ.back().fsize != -1)
				cout << "File size aleady set" << endl;
			else 
				client.fileQ.back().fsize = fsize;
		}
		else {
			if (client.upfile.back().fsize != -1) 
				cout << "UpFile size aleady set" << endl;
			else 
				client.upfile.back().fsize = fsize;
		}
	}	
	if (userAllFile[uname].fileQ.back().fsize != -1)
		cout << "AllFile size aleady set" << endl;
	else 
		userAllFile[uname].fileQ.back().fsize = fsize;
}
void addclient() {
	sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int sockfd = accept(listenfd, (sockaddr*)&cliaddr, &clilen);
	Msg msg = myread(sockfd);
	if (msg.mod != UNAME) {
		cout << "[Error] Should receive user name" << endl;
		return ;
	}
	msg.buf[msg.len] = 0;
	string name = msg.buf;
	Client tmpClient = userAllFile[name];
	tmpClient.sockfd = sockfd;
	users[name].push_back(tmpClient);
	FD_SET(sockfd, &all_set);
	maxfd = max(sockfd, maxfd);
	int flag = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flag|O_NONBLOCK);
	int LOWAT = sizeof(Msg);
	if (setsockopt(listenfd, SOL_SOCKET, SO_RCVLOWAT, &LOWAT, sizeof(int)) < 0) {
		cout << "[Error] Set RCVLOWAT failed" << endl;
	}
}
void rmclient(string uname, int sockfd) {
	cout << "Client " << uname << ":" << sockfd << " is offline" << endl;
	for (int i = 0; i < users[uname].size(); ++i) {
		if (users[uname][i].sockfd == sockfd) {
			FD_CLR(sockfd, &all_set);
			users[uname].erase(users[uname].begin() + i);
			return ;
		}
	}
}
int main(int argc, char** argv) {
	if (argc < 2) {
		cout << "Usage: " + string(argv[0]) + " <port>" << endl;
		exit(EXIT_FAILURE);
	}
	sockaddr_in servaddr;	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(atoi(argv[1]));

	FD_ZERO(&all_set);
	FD_ZERO(&r_set);
	FD_ZERO(&w_set);

	int yes = 1;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	FD_SET(listenfd, &all_set);
	maxfd = listenfd;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
		cout << "[Error] Set REUSEADDR failed" << endl;
	}

	if (::bind(listenfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0 ) {
		cout << "Failed to bind" << endl;
		exit(EXIT_FAILURE);
	}
	listen(listenfd, 32);	

	fcntl(STDIN_FILENO, F_SETFL, 2);
	fcntl(STDOUT_FILENO, F_SETFL, 2);
	
	system(("mkdir -p " + SERVDIR).c_str());

	while (true) {
		r_set = all_set;
		w_set = all_set;
		int nready = select(maxfd+1, &r_set, &w_set, NULL, NULL);
		if (FD_ISSET(listenfd, &r_set)) {
			addclient();	
		}
		for (auto&& user:users) {
			for (auto&& client:user.second) {
				if (FD_ISSET(client.sockfd, &r_set)) {
					Msg msg = myread(client.sockfd);			
					if (msg.mod == CONNCLOSE) {
						rmclient(user.first, client.sockfd);
					} else if (msg.mod == FNAME) {
						msg.buf[msg.len] = 0;
						string fname = SERVDIR + "/" + msg.buf;
						int fd = creat(fname.c_str(), 0644);
						close(fd);
						addfile(user.first, msg.buf, client.sockfd);
					} else if (msg.mod == FSIZE) {
						setfsize(user.first, msg.len, client.sockfd);
					} else if (msg.mod == FDATA) {
						if (client.upfile.size() != 1) {
							cout << "[Error] Upfile number error" << endl;
							continue;
						}
						string fname = SERVDIR + "/" + client.upfile.front().fname;
						ofstream fout(fname, ofstream::app);
						fout.write(msg.buf, msg.len);
						fout.close();
					} else if (msg.mod == MYEOF) {
						client.upfile.pop();
					}
				} else if (FD_ISSET(client.sockfd, &w_set)) {
					if ((!client.fileQ.empty()) || client.msgQ) { // not done
						if (!client.msgQ) { // msgQ empty -> put something in
							if (client.fileQ.front().nextMsg == FNAME) {
								client.msg = Msg(FNAME, client.fileQ.front().fname.size(), client.fileQ.front().fname.c_str());
								client.fileQ.front().nextMsg = FSIZE;
								client.msgQ = true;
								client.nextMp = 0;
							} else if (client.fileQ.front().nextMsg == FSIZE) {
								if (client.fileQ.front().fsize == -1) {
									client.msgQ = false;
									client.nextMp = 0;
								} else {
									client.msg = Msg(FSIZE, client.fileQ.front().fsize, 0);	
									client.fileQ.front().nextMsg = FDATA;
									client.msgQ = true;
									client.nextMp = 0;
								}
							} else if (client.fileQ.front().nextMsg == FDATA) {
								ifstream fin(SERVDIR + "/" + client.fileQ.front().fname);
								fin.seekg(client.fileQ.front().nextFp);	
								char buf[MAXLINE];
								int n = fin.readsome(buf, MAXLINE);
								client.msg = Msg(FDATA, n, buf);
								int g = fin.tellg();
								if (g == client.fileQ.front().fsize) {
									client.fileQ.front().nextMsg = MYEOF;
								} else {
									client.fileQ.front().nextFp = g;
									client.fileQ.front().nextMsg = FDATA;
								}
								//update nextMsg		
								client.msgQ = true;
								client.nextMp = 0;
							} else if (client.fileQ.front().nextMsg == MYEOF) {
								client.msg = Msg(MYEOF, 0, 0);
								client.fileQ.front().nextMsg = FEND;
								client.msgQ = true;
								client.nextMp = 0;
							}
							//client.msgQ = true;
							//client.nextMp = 0;
						}
						if (client.msgQ) {
							int n = write(client.sockfd, ((char*)&client.msg)+client.nextMp, sizeof(client.msg)-client.nextMp);
							if (n < 0) {
								cout << "[Error] Write Failed" << endl;
							} else {
								client.nextMp += n;
								if (client.nextMp == sizeof(client.msg)) {
									client.msgQ = false;
									client.nextMp = 0;
									if (client.fileQ.front().nextMsg == FEND)
										client.fileQ.pop();
								}
								else cout << "Msg not complete" << endl;
							}
						}
					}
				}
			}
		}
	}
	/*
	sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int sockfd = accept(listenfd, (sockaddr*)&cliaddr, &clilen);
	FD_SET(sockfd, &all_set);
	maxfd = max(sockfd, listenfd);
	int LOWAT = MAXLINE;
	setsockopt(listenfd, SOL_SOCKET, SO_RCVLOWAT, &LOWAT, sizeof(int));
	while (true) {
		r_set = all_set;
		int fd;
		int nready = select(maxfd+1, &r_set, NULL, NULL, NULL);
		if (FD_ISSET(sockfd, &r_set)) {
			char buf[1024];
			Msg msg = myread(sockfd);
			if (msg.mod == CONNCLOSE) {
				cout << "Connection closed" << endl;
				break;
			} else if (msg.mod == FNAME) {
				msg.buf[msg.len] = 0;
				fd = creat("msg.buf", 0644);
			} else if (msg.mod == UNAME) {
				msg.buf[msg.len] = 0;
			} else {
				cout << getMod(msg.mod) << " len:" << msg.len << " data:" << flush;
				if (msg.mod != FSIZE) write(fd, msg.buf, msg.len);
				cout << "...................." << endl;
			}
		}
	}
	*/
	return 0;
}
