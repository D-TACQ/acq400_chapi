/*
 * acq400_chapi.h
 *
 *  Created on: 24 Apr 2021
 *      Author: pgm
 */

#ifndef ACQ400_CHAPI_ACQ400_CHAPI_H_
#define ACQ400_CHAPI_ACQ400_CHAPI_H_

#include <vector>
#include <map>
#include <string>

namespace acq400_chapi {

class Netclient {

protected:
	const char* addr;
	const int port;
	const int skt;
	std::vector<char> buffer;
public:
	Netclient(const char* _addr, int _port);
	virtual ~Netclient();
	int receive_message(char* rx_message, int max_rx, const char* termex);
};


class Siteclient: public Netclient {
	bool trace;
public:
	Siteclient(const char* addr, int port);
	int sr(char* rx_message, int max_rx, const char* txfmt, ...);
	int _sr(char* rx_message, int max_rx, const char* tx);
};



class Acq400 {
	const char* uut;
public:
	std::map<std::string, Siteclient*>sites;

	Acq400(const char* _uut);

	virtual int set(std::string* response, const std::string& site, const char* fmt, ...);
	virtual int get(std::string* response, const std::string& site, const char* fmt, ...);
};

} 	// namespace acq400_chapi

#endif /* ACQ400_CHAPI_ACQ400_CHAPI_H_ */
