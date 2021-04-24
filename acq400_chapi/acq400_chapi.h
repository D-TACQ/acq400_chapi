/*
 * acq400_chapi.h
 *
 *  Created on: 24 Apr 2021
 *      Author: pgm
 */

#ifndef ACQ400_CHAPI_ACQ400_CHAPI_H_
#define ACQ400_CHAPI_ACQ400_CHAPI_H_

#include <vector>

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
public:
	Siteclient(const char* addr, int port);
	int sr(char* rx_message, int max_rx, const char* txfmt, ...);
};

}

#endif /* ACQ400_CHAPI_ACQ400_CHAPI_H_ */
