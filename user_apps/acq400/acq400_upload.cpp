/*
 * acq400_upload.cpp
 *
 *  Created on: 21 Oct 2021
 *      Author: pgm
 */


#include <stdio.h>
#include <unistd.h>
#include "acq400_chapi.h"

#include <string>
#include <iostream>
#include <bits/stdc++.h>

#include <popt.h>

#define BUFLEN 0x10000

namespace G {
	const char* uut;
	int data32;

	int upload = 0;

	int post = 100000;
}

struct poptOption opt_table[] = {
	{ "post",      'P', POPT_ARG_INT, &G::post, 0   },
	{ "upload",    'u', POPT_ARG_INT, &G::upload, 0 },
	POPT_AUTOHELP
	POPT_TABLEEND
};


void ui(int argc, const char **argv) {
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;
	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		;
	}
	G::uut = poptGetArg(opt_context);
	if (!G::uut){
		fprintf(stderr, "ERROR: must provide uut\n");
		exit(1);
	}
}

void init(acq400_chapi::Acq400& uut) {
	/*
	for (const auto& [key, value] : uut.sites) {
		//std::cout << key << std::endl;
		std::string model;
		uut.get(&model, key, "MODEL");
		std::cout << "s" << key << " MODEL:" << model << std::endl;
	}
	 */

}

/*
set.site 0 transient "PRE=0 POST=50000 DEMUX=0"

set.site 0 set_arm
*/

void capture(acq400_chapi::Acq400& uut) {
	std::string state("state");
	std::string ostate("ostate");

	std::string rx(128, 'x');
	std::string site("0");
	uut.set(&rx, site, "transient POST=%d DEMUX=0", G::post);
	uut.set(&rx, site, "set_arm %d", 1);

	bool first_time = true;
	while (first_time || state == "TRANS_ACT:STATE IDLE"){
		ostate = state;
		uut.get(&state, site, "TRANS_ACT:STATE");
		if (ostate != state){
			printf("%s -> %s\n", ostate.c_str(), state.c_str());
		}
		usleep(100000);
		first_time = false;
	}
	while (state != "TRANS_ACT:STATE IDLE"){
		ostate = state;
		uut.get(&state, site, "TRANS_ACT:STATE");
		if (ostate != state){
			printf("%s -> %s\n", ostate.c_str(), state.c_str());
		}
		usleep(100000);
	}
}

template <class C>
int upload(acq400_chapi::Acq400& uut, FILE* fout) {
	C* buf = new C[BUFLEN];
	int nbuf;
	int total = 0;
	while ((nbuf = uut.stream(buf, BUFLEN, acq400_chapi::Ports::DATA0)) > 0){
		total += fwrite(buf, sizeof(C), nbuf, fout);
	}
	return total;
}

int upload(acq400_chapi::Acq400& uut)
{
	FILE* fout = fopen("rawdata.dat", "w");
	if (G::data32){
		return upload<long>(uut, fout);
	}else{
		return upload<short>(uut, fout);
	}
}

int main(int argc, const char **argv) {
	ui(argc, argv);
	acq400_chapi::Acq400 uut(G::uut);
	init(uut);
	capture(uut);

	if (G::upload){
		upload(uut);
	}

	return 0;
}
