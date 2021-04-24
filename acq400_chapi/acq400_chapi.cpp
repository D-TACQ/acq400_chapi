/*
 * acq400_chapi.cpp
 *
 *  Created on: 24 Apr 2021
 *      Author: pgm
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>


#include <regex>

#include "acq400_chapi.h"

namespace acq400_chapi {

void error(const char *format, ...) {
	char buf[128];
	va_list args;
	va_start (args, format);
	vsnprintf (buf, 128, format, args);
	va_end (args);
	perror(buf);
	exit(1);
}

static int connect(const char* addr, int port)
{
	int sockfd;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
		error("ERROR opening socket");
	}
	server = gethostbyname(addr);
	if (server == NULL) {
		error("ERROR, no such host as %s\n", addr);
	}

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,
			(char *)&serveraddr.sin_addr.s_addr, server->h_length);
	serveraddr.sin_port = htons(port);

	/* connect: create a connection with the server */
	if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0){
		error("ERROR connecting");
	}
	return sockfd;
}


Netclient::Netclient(const char* _addr, int _port):
	addr(_addr), port(_port), skt(connect(_addr, _port)), buffer(4096)
{
	buffer.reserve(4096);
}

Netclient::~Netclient() {

}

int Netclient::receive_message(char* rx_message, int max_rx, const char* termex)
{
	char buf[1025];
	int nrx = buffer.size();

	while(1){
		int rc = recv(skt, buf, 1024, 0);
		if (rc <= 0){
			error("shutdown\n");
		}
		nrx += rc;
		buf[rc] = '\0';
		printf("<%s\n", buf);
		if (strstr(buf, termex)){
			for (int ii = 0; ii < termex-buf+strlen(termex); ++ii){
				buffer.push_back(buf[ii]);
				if (nrx > max_rx){
					nrx = max_rx;
				}

				strncpy(rx_message, buffer.data(), nrx);
				buffer.clear();
				printf("copy %d: %s\n", nrx, rx_message);
				// add any more stuff to buffer?
				return nrx;
			}
		}else{
			for (int ii = 0; ii < rc; ++ii){
				buffer.push_back(buf[ii]);
			}
		}
	}

	return 0;
}

Siteclient::Siteclient(const char* _addr, int _port) : Netclient(_addr, _port) {
	char gash[64];
	sr(gash, 64, "prompt on");
}
int Siteclient::sr(char* rx_message, int max_rx, const char* txfmt, ...)
{
	char lbuf[130];
	int rc;
	va_list args;
	va_start (args, txfmt);
	vsnprintf (lbuf, 128, txfmt, args);
	va_end (args);
	strcat(lbuf, "\n");
	printf("> %s", lbuf);
	rc = send(skt, lbuf, ::strlen(lbuf), 0);
	if (rc != strlen(lbuf)){
		error("send tried %d returned %d\n", strlen(lbuf), rc);
	}
	if (max_rx){
		rc = receive_message(rx_message, max_rx, ">");
	}else{
		rc = 0;
	}

	return rc;
}


}
