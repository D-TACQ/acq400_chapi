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

enum Ports {
	TSTAT = 2235,
	STREAM = 4210,
	SITE0 = 4220,
	SEGSW = 4250,
	SEGSR = 4251,
	DPGSTL = 4521,
	GPGSTL= 4541,
	GPGDUMP = 4543,

	WRPG = 4606,

	DIO482_PG_STL = 45001,
	DIO482_PG_DUMP = DIO482_PG_STL+2,

	BOLO8_CAL = 45072,
	DATA0 = 53000,
	DATAT = 53333,
	MULTI_EVENT_TMP = 53555,
	MULTI_EVENT_DISK = 53556,
	DATA_SPY = 53667,
	LIVETOP = 53998,
	ONESHOT = 53999,
	AWG_ONCE = 54201,
	AWG_AUTOREARM = 54202,
	AWG_CONTINUOUS = 54205,
	AWG_STREAM = 54207,
	MGTDRAM = 53993,
	MGTDRAM_PULL_DATA = 53991
};

class Acq400 {
	const char* uut;

	FILE *fstream;
	FILE* stream_open(enum Ports port);
public:
	std::map<std::string, Siteclient*>sites;

	Acq400(const char* _uut);

	virtual int set(std::string* response, const std::string& site, const char* fmt, ...);
	virtual int get(std::string* response, const std::string& site, const char* fmt, ...);

	virtual int set(const std::string& site, const char* key, int value);
	virtual int get(const std::string& site, const char* key, int& value);

	virtual int stream(short buf[], int maxbuf, enum Ports port=STREAM);
	virtual int stream(long buf[],  int maxbuf, enum Ports port=STREAM);
};

} 	// namespace acq400_chapi

#endif /* ACQ400_CHAPI_ACQ400_CHAPI_H_ */
