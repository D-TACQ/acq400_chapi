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
#include "acq400_chapi_inc.h"

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
	std::regex term_regex(termex);

	while(1){
		int rc = recv(skt, buf, 1024, 0);
		if (rc <= 0){
			error("shutdown\n");
		}

		buf[rc] = '\0';
		//printf("<%s\n", buf);

		std::cmatch cm;
		if (std::regex_search(buf, cm, term_regex)){
			//printf("match cursor:%d string:\"%s\"\n", cm.position(), buf);
			nrx += cm.position();
			for (int ii = 0; ii < cm.position(); ++ii){
				buffer.push_back(buf[ii]);
			}
			if (nrx >= max_rx){
				nrx = max_rx;
			}
			while(buffer.data()[nrx-1] =='\n'){
				--nrx;
			}
			strncpy(rx_message, buffer.data(), nrx);
			rx_message[nrx] = '\0';
			buffer.clear();
			//printf("copy %d: \"%s\"\n", nrx, rx_message);
			// add any more stuff to buffer?
			return nrx;
		}else{
			for (int ii = 0; ii < rc; ++ii){
				buffer.push_back(buf[ii]);
			}
			nrx += rc;
		}
	}

	return 0;
}


int getenvint(const char* key, int def)
{
	const char *val = ::getenv(key);
	if (val){
		return strtol(val, 0, 0);
	}else{
		return def;
	}
}

Siteclient::Siteclient(const char* _addr, int _port) :
		Netclient(_addr, _port), trace(getenvint("SITECLIENT_TRACE", 0))
{
	char gash[64];
	sr(gash, 64, "prompt on");
}

int Siteclient::_sr(char* rx_message, int max_rx, const char* tx)
{
	if (trace) fprintf(stderr, ">%s", tx);
	int rc = send(skt, tx, ::strlen(tx), 0);
	if (rc != strlen(tx)){
		error("send tried %d returned %d\n", strlen(tx), rc);
	}
	if (max_rx){
		rc = receive_message(rx_message, max_rx, "(acq400.[0-9]+ ([0-9]+) >)");
		if (trace) fprintf(stderr, "<%s\n", rx_message);
	}else{
		rc = 0;
	}

	return rc;
}
int Siteclient::sr(char* rx_message, int max_rx, const char* txfmt, ...)
{
	char lbuf[130];

	va_list args;
	va_start (args, txfmt);
	vsnprintf (lbuf, 128, txfmt, args);
	va_end (args);
	strcat(lbuf, "\n");
	return _sr(rx_message, max_rx, lbuf);
}

Acq400::Acq400(const char* _uut): uut(_uut), fstream(0) {
	Siteclient* s0 = new Siteclient(uut, 4220);
	char _sitelist[80];
	s0->sr(_sitelist, 80, "sites");
	sites["0"] = s0;

	std::vector<std::string> sitelist;
	split(_sitelist, ',', sitelist);
	for (int i = 0; i < sitelist.size(); i++){
		sites[sitelist[i]] = 0;	// lazy init
	}
};


int Acq400::set(std::string* response, const std::string& site, const char* fmt, ...)
{
	char lbuf[130];
	char rx_message[16384];

	va_list args;
	va_start (args, fmt);
	vsnprintf (lbuf, 128, fmt, args);
	va_end (args);
	strcat(lbuf, "\n");

	Siteclient* sc = sites[site];
	if (sc == 0){
		sites[site] = sc = new Siteclient(uut, 4220+atoi(site.c_str()));
	}

	int rc = sc->_sr(rx_message, 16384, lbuf);
	if (rc > 0 && response){
		response->append(rx_message);
	}
	return rc;
}
int Acq400::get(std::string* response, const std::string& site, const char* fmt, ...)
{
	char lbuf[130];
	char rx_message[16384];

	va_list args;
	va_start (args, fmt);
	vsnprintf (lbuf, 128, fmt, args);
	va_end (args);
	strcat(lbuf, "\n");
	if (response){
		response->clear();
	}

	Siteclient* sc = sites[site];
	if (sc == 0){
		sites[site] = sc = new Siteclient(uut, 4220+atoi(site.c_str()));
	}

	int rc = sc->_sr(rx_message, 16384, lbuf);
	if (rc > 0 && response){
		response->append(rx_message);
	}
	return rc;
}

int Acq400::set(const std::string& site, const char* key, int value)
{
	char lbuf[130];
	char rx_message[16384];
	Siteclient* sc = sites[site];

	snprintf(lbuf, 130, "%s=%d\n", key, value);
	if (sc == 0){
		sites[site] = sc = new Siteclient(uut, 4220+atoi(site.c_str()));
	}
	int rc = sc->_sr(rx_message, 16384, lbuf);
	return rc;
}

int Acq400::get(const std::string& site, const char* key, int& value)
{
	char rx_message[16384];
	char lbuf[130];
	Siteclient* sc = sites[site];
	if (sc == 0){
		sites[site] = sc = new Siteclient(uut, 4220+atoi(site.c_str()));
	}
	snprintf(lbuf, 130, "%s\n", key);
	int rc = sc->_sr(rx_message, 16384, lbuf);
	if (rc > 0){
		return sscanf(rx_message, "%d", &value);
	}
	return rc;
}
FILE* Acq400::stream_open(enum Ports port, const char* mode)
{
	if (fstream == 0){
		int stream_socket = connect(uut, port);
		if (stream_socket < 0){
			perror("stream socket");
			exit(errno);
		}
		fstream = fdopen(stream_socket, mode);
	}
	return fstream;
}
int Acq400::stream(short buf[], int maxbuf, enum Ports port)
{
	return fread(buf, sizeof(short), maxbuf, stream_open(port));
}
int Acq400::stream(long buf[],  int maxbuf, enum Ports port)
{
	return fread(buf, sizeof(long), maxbuf, stream_open(port));
}

int Acq400::stream_out(short buf[], int maxbuf, enum Ports port)
{
	return fwrite(buf, sizeof(short), maxbuf, stream_open(port, "w"));
}
int Acq400::stream_out(long buf[],  int maxbuf, enum Ports port)
{
	return fwrite(buf, sizeof(long), maxbuf, stream_open(port, "w"));
}


}	// namespace acq400_chapi
	
// https://stackoverflow.com/questions/275404/splitting-strings-in-c
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

