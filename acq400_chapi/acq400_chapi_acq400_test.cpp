/*
 * acq400_chapi_acq400_test.cpp
 *
 *  Created on: 25 Apr 2021
 *      Author: pgm
 */


#include <stdio.h>
#include "acq400_chapi.h"

#include <string>
#include <iostream>
#include <bits/stdc++.h>


int main(int argc, char **argv) {
	if (argc < 2){
		fprintf(stderr, "USAGE: acq400_chapi_acq400_test UUT [command]\n");
		exit(1);
	}
	const char* host = argv[1];

	acq400_chapi::Acq400 uut(host);

	for (const auto& [key, value] : uut.sites) {
		//std::cout << key << std::endl;
		std::string model;
		uut.get(&model, key, "MODEL");
		std::cout << "s" << key << " MODEL:" << model << std::endl;
	}
/*
	if (argc > 2){
		for (int ic = 3; ic < argc; ++ic){
			std::string rx_message
			int rc = uut.get(rx_message, argv[ic]);
			if (rc > 0){
				cout << "result " << rx_message << endl;
			}else{
				fprintf(stderr, "ERROR: sr returned %d\n", rc);
			}
		}
	}else{
		std::string tx_message;
		int ic = 0;

		printf("ready %d>", ic); fflush(stdout);
		while(std::getline(std::cin, tx_message)){
			int rc = site.sr(rx_message, 16384, "%s\n", tx_message.c_str());
			if (rc > 0){
				printf("%s\n", rx_message);
			}else{
				fprintf(stderr, "ERROR: sr returned %d\n", rc);
			}
			printf("ready %d>", ++ic); fflush(stdout);
		}
	}
*/
	return 0;
}

