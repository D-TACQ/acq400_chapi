/*
 * acq400_stream_awg.cpp
 *
 *  Created on: 3 September 2021
 *
 *  cat awg_data_file | acq400_stream_awg UUT[:port]
 *  default port: 54207
 *  streaming function stands in for
 *  cat awg_data_file | nc UUT 54207
 *
 *  also
 *  USAGE: acq400_stream_awg UUT[:PORT] [repeat] [file]
 *      Author: pgm
 */



#include "acq400_chapi.h"
#include "acq400_chapi_inc.h"

#define BUFLEN 0x10000

typedef std::vector<FILE*> FPV;

template <class C>
int streamer(acq400_chapi::Acq400& uut, acq400_chapi::Ports port, FPV& files, bool repeat)
{
	C* buf = new C[BUFLEN];
	int nbuf;
	int skt = 0;

	do {
		for (auto fp: files){
			while ((nbuf = fread(buf, sizeof(C), BUFLEN, fp)) > 0){
				uut.stream_out(&skt, buf, nbuf, port);
			}
			if (repeat){
				rewind(fp);
			}
		}
	} while(repeat);

	close(skt);
	return 0;
}

#define USAGE "USAGE: acq400_stream_awg UUT[:PORT] [repeat] [file]\n"

int main(int argc, char **argv) {
	if (argc < 2){
		fprintf(stderr, USAGE);
		exit(1);
	}
	acq400_chapi::Ports port = acq400_chapi::AWG_STREAM;
	FPV files;
	const char* host;


	bool repeat = false;
	std::vector<std::string> host_port;

	split(argv[1], ':', host_port);
	host = host_port[0].c_str();
	if (host_port.size() > 1){
		port = static_cast<acq400_chapi::Ports>(atoi(host_port[1].c_str()));
	}
	printf("port set %d\n", port);

	if (argc > 2){
		int i1 = 2;
		if (strcmp(argv[2], "repeat") == 0){
			if (argc > 3){
				i1 = 3;
				repeat = true;
			}else{
				fprintf(stderr, "ERROR: repeat only supported with file\n");
				exit(1);
			}
		}
		for (int ii = i1; ii < argc; ++ii){
			const char* fname = argv[ii];

			FILE* fp = fopen(fname, "rb");
			if (fp == 0){
				fprintf(stderr, "ERROR failed to open file \"%s\"\n", fname);
				exit(1);
			}
			files.push_back(fp);
		}

	}else{
		files.push_back(stdin);
	}

	acq400_chapi::Acq400 uut(host);
	int data32;
	if (uut.get("0", "data32", data32) < 0){
		fprintf(stderr, "ERROR:");
		exit(1);
	}

	return (data32? streamer<long>: streamer<short>)(uut, port, files, repeat);
}

