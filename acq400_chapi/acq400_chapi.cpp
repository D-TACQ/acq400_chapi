/*
 * acq400_chapi.cpp
 *
 *  Created on: 24 Apr 2021
 *      Author: pgm
 */







#include "acq400_chapi.h"
#include "acq400_chapi_inc.h"

#include <stdarg.h>

#include <string.h>
#include <strings.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <netdb.h>
#include <errno.h>

#include <regex>
#include <poll.h>
#include <fcntl.h>

namespace acq400_chapi {

int getenvint(const char* key, int def)
{
	const char *val = ::getenv(key);
	if (val){
		return strtol(val, 0, 0);
	}else{
		return def;
	}
}


static int G_verbose = getenvint("ACQ400_CHAPI_VERBOSE", 0);

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
		if (G_verbose) printf("<%s\n", buf);

		std::cmatch cm;
		if (std::regex_search(buf, cm, term_regex)){
			if (G_verbose>1) printf("match cursor:%ld string:\"%s\"\n", cm.position(), buf);
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
			if (G_verbose>1) printf("copy %d: \"%s\"\n", nrx, rx_message);
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


int Acq400::set(std::string& response, const std::string& site, const char* fmt, ...)
{
	char lbuf[130];
	char rx_message[16384];

	va_list args;
	va_start (args, fmt);
	vsnprintf (lbuf, 128, fmt, args);
	va_end (args);
	strcat(lbuf, "\n");

	//printf("Acq400::set site %s\n", site.c_str());

	Siteclient* sc = sites[site];
	if (sc == 0){
		sites[site] = sc = new Siteclient(uut, 4220+atoi(site.c_str()));
	}

	int rc = sc->_sr(rx_message, 16384, lbuf);
	if (rc > 0){
		response.append(rx_message);
	}
	return rc;
}
int Acq400::get(std::string& response, const std::string& site, const char* fmt, ...)
{
	char lbuf[130];
	char rx_message[16384];

	va_list args;
	va_start (args, fmt);
	vsnprintf (lbuf, 128, fmt, args);
	va_end (args);
	strcat(lbuf, "\n");

	response.clear();

	//printf("Acq400::get site %s\n", site.c_str());

	Siteclient* sc = sites[site];
	if (sc == 0){
		sites[site] = sc = new Siteclient(uut, 4220+atoi(site.c_str()));
	}

	int rc = sc->_sr(rx_message, 16384, lbuf);
	if (rc > 0){
		response.append(rx_message);
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
	return fread(buf, sizeof(short), maxbuf, stream_open(port, "r"));
}
int Acq400::stream(long buf[],  int maxbuf, enum Ports port)
{
	return fread(buf, sizeof(long), maxbuf, stream_open(port, "r"));
}

int Acq400::stream_open(enum Ports port)
{
	int rc = connect(uut, port);
	if (G_verbose) printf("stream_open() %d ret %d\n", port, rc);
	return rc;
}

const int STREAM_TO = 5000;  // 5 seconds

int Acq400::stream_out(int* pskt, char buf[], int maxbuf, enum Ports port)
{
	if (*pskt == 0) {
		*pskt = stream_open(port);
	}
	int skt = *pskt;

	int nbytes = 0;

	if (maxbuf == 0){
		shutdown(skt, SHUT_WR);
	}
	while(nbytes < maxbuf || maxbuf == 0){
		struct pollfd fds[1];
		int timeout = STREAM_TO;
		fds[0].fd = skt;
		fds[0].events = POLLIN | POLLOUT; // Monitor for read and write
		int ret = poll(fds, 1, timeout);
		if (ret == -1) {
			perror("poll");
 			close(skt);
		        return -1;
		} else if (ret == 0) {
		        printf("Timeout occurred! No events in %d msec.\n", STREAM_TO);
		} else {
		        if (fds[0].revents & POLLIN) {
				char rbuf[256];
				int nr = read(skt, rbuf, 256);
				if (nr > 0){
					rbuf[nr] = '\0';
					rbuf[strcspn(rbuf, "\n")] = '\0';
					printf("<%s\n", rbuf);
				}else{
					printf("POLLIN, no bytes? %d\n", nr);
				}
			}	
       			if (maxbuf != 0 && fds[0].revents & POLLOUT) {
				int wbytes = write(skt, (char*)buf+nbytes, maxbuf-nbytes);
				if (wbytes > 0){
					nbytes += wbytes;
					if (G_verbose) printf("write %d %d %d %d\n", __LINE__, skt, maxbuf, wbytes);
				}else{
					perror("write");
				}
			}
        		if (fds[0].revents & POLLERR) {
		        	printf("Error condition on the file descriptor.\n");
				return -1;
        		}
		        if (fds[0].revents & POLLHUP) {
				if (maxbuf == 0){
					close(skt);
					return 0;
				}
			        printf("Hang up on the file descriptor.\n");
				return -1;
		        }
		}
	}
	return nbytes;
}
int Acq400::stream_out(int* pskt, short buf[], int maxbuf, enum Ports port)
{
	int nbytes = stream_out(pskt, (char*)buf, maxbuf*sizeof(short), port);
	if (nbytes <= 0){
		return nbytes;
	}else{
		return nbytes/sizeof(short);
	}
}
int Acq400::stream_out(int* pskt, long buf[],  int maxbuf, enum Ports port)
{
	int nbytes = stream_out(pskt, (char*)buf, maxbuf*sizeof(long), port);
	if (nbytes <= 0){
		return nbytes;
	}else{
		return nbytes/sizeof(long);
	}
}


void Acq400::select_awg_seg(int* pskt, acq400_chapi::Acq400& uut, char seg)
{
	if (*pskt == 0){
		int flag = 1;
		*pskt = uut.stream_open(AWG_SEG_SEL);
		setsockopt(*pskt, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
	}
	stream_out(pskt, &seg, 1, AWG_SEG_SEL);
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

