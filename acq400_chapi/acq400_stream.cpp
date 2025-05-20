/*
 * acq400_stream.cpp
 *
 *  Created on: 28 July 2021
 *
 *  acq400_stream UUT [port] | pv > mybigrawfile
 *  streaming function stands in for
 *  nc UUT 4210 | pv > mybigrawfile
 *      Author: pgm
 */


#include "acq400_chapi.h"
#include "acq400_chapi_inc.h"


#define BUFLEN 0x10000

namespace G {
	unsigned long long maxbytes = 0;
};

template <class C>
int streamer(acq400_chapi::Acq400& uut, acq400_chapi::Ports port)
{
	C* buf = new C[BUFLEN];
	int nbuf;
	unsigned long long nbytes = 0;

	while ((nbuf = uut.stream(buf, BUFLEN, port)) > 0){
		fwrite(buf, sizeof(C), nbuf, stdout);
		nbytes += nbuf*sizeof(C);
		if (G::maxbytes && nbytes >= G::maxbytes){
			break;
		}
	}
	return 0;
}

void set_data_mode(acq400_chapi::Acq400& uut, int pack24)
{
	int rc = uut.set("1", "pack24", pack24);
	assert(rc == 0);
}

#define ENABLE 1
#define RISING 1

void set_trigger_mode(acq400_chapi::Acq400& uut, int soft_trigger)
{
	std::string response;
	int rc = uut.set(response, "1", "trg=%d,%d,%d", ENABLE, soft_trigger, RISING);
	assert(rc == 0);
}

void set_uut_mode(acq400_chapi::Acq400& uut){
	int pack24 = 0;
	if (getenv("ACQ400_PACK24")){
		pack24 = atoi(getenv("ACQ400_PACK24"));
	}
	set_data_mode(uut, pack24);

	int soft_trigger = 0;
	if (getenv("ACQ400_SOFT_TRIGGER")){
		soft_trigger = atoi(getenv("ACQ400_SOFT_TRIGGER"));
	}

	set_trigger_mode(uut, soft_trigger);
}

int main(int argc, char **argv) {
	if (argc < 2){
		fprintf(stderr, "USAGE: acq400_stream UUT [PORT]\n");
		exit(1);
	}
	if (getenv("ACQ400_STREAM_MAXB")){
		G::maxbytes = strtoull(getenv("ACQ400_STREAM_MAXB"), 0, 0);
	}
	if (getenv("ACQ400_STREAM_MAXM")){
		G::maxbytes = strtoull(getenv("ACQ400_STREAM_MAXM"), 0, 0) * 0x100000;
	}

	const char* host = argv[1];
	acq400_chapi::Ports port = acq400_chapi::STREAM;
	if (argc > 2){
		acq400_chapi::Ports port = static_cast<acq400_chapi::Ports>(atoi(argv[2]));
	}

	acq400_chapi::Acq400 uut(host);

	set_uut_mode(uut);

	int data32;
	if (uut.get("0", "data32", data32) < 0){
		fprintf(stderr, "ERROR:");
		exit(1);
	}

	if (data32){
		return streamer<long>(uut, port);
	}else{
		return streamer<short>(uut, port);
	}

}

