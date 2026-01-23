/*
 * acq400_chapi.h
 *
 *  Created on: 24 Apr 2021
 *      Author: pgm
 */

#ifndef ACQ400_CHAPI_ACQ400_CHAPI_H_
#define ACQ400_CHAPI_ACQ400_CHAPI_H_

#include <assert.h>
#include <string>
#include <map>
#include <vector>


#define IN_RANGE(x, l, r)  ((x)>=(l) && (x) <=(r))

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
	MGTDRAM_PULL_DATA = 53991,

	AWG_SEG_SEL   = 54210,   /* select next segment */
	AWG_SEG_ARP_A = 54212,   /* one-shot Auto-RePeat */
	AWG_SEG_CON_A = 54215,	 /* CONtinuous */

	AWG_CH_01 = 54001        /* load by channel */
};

inline
enum Ports AWG_SEG_ARP(char seg) {
	assert(seg >= 'A' && seg <= 'Z');
	return (enum Ports)(AWG_SEG_ARP_A + 10*(seg-'A'));
}

inline
enum Ports AWG_SEG_CON(char seg) {
	assert(seg >= 'A' && seg <= 'Z');
	return (enum Ports)(AWG_SEG_CON_A + 10*(seg-'A'));
}

enum { RC_SUCCESS=0 };


class Acq400 {
	FILE *fstream;
	FILE* stream_open(enum Ports port, const char* mode);
	int stream_open(enum Ports port);
public:
	const char* uut;
	std::map<std::string, Siteclient*>sites;

	Acq400(const char* _uut);

	virtual int set(std::string& response, const std::string& site, const char* fmt, ...);
	virtual int get(std::string& response, const std::string& site, const char* fmt, ...);

	virtual int set(const std::string& site, const char* key, int value);
	/* returns 0 if it worked */
	virtual int get(const std::string& site, const char* key, int& value);
	/* returns RC_SUCCESS if it worked */

	virtual int stream(short buf[], int maxbuf, enum Ports port=STREAM);
	virtual int stream(long buf[],  int maxbuf, enum Ports port=STREAM);


	virtual void select_awg_seg(int* pskt, char seg);
	virtual void stop_awg();
	/* request awg_stop */
	virtual void set_playloop_len_disable(bool disable);
	/* request awg_stop on next load */

	virtual int stream_out(int* pskt, char buf[], int maxbuf, enum Ports port=AWG_STREAM);
	virtual int stream_out(int* pskt, short buf[], int maxbuf, enum Ports port=AWG_STREAM);
	virtual int stream_out(int* pskt, long buf[],  int maxbuf, enum Ports port=AWG_STREAM);
};
	const int AWG_LOADER_BUFLEN = 0x10000;

	template <class C>
	int awg_loader(Acq400& uut, Ports port, FILE* fp){
		C* buf = new C[AWG_LOADER_BUFLEN];
		int nbuf = 0;
		int skt = 0;

		while ((nbuf = fread(buf, sizeof(C), AWG_LOADER_BUFLEN, fp)) > 0){
			uut.stream_out(&skt, buf, nbuf, port);
		}

		uut.stream_out(&skt, buf, 0, port);
		return 0;
	}

} 	// namespace acq400_chapi

#endif /* ACQ400_CHAPI_ACQ400_CHAPI_H_ */
