
#include <stdio.h>
#include "acq400_chapi.h"

#include <string>
#include <iostream>
#include <bits/stdc++.h>


char rx_message[16384];


int main(int argc, char **argv) {
	if (argc < 3){
		fprintf(stderr, "USAGE: acq400_chapi_siteclient_test UUT PORT [command]\n");
		exit(1);
	}
	const char* host = argv[1];
	int port = atoi(argv[2]);
	printf("connect to %s:%d\n", host, port);

	acq400_chapi::Siteclient site(host, port);
	if (argc > 3){
		for (int ic = 3; ic < argc; ++ic){
			int rc = site.sr(rx_message, 16384, argv[ic]);
			if (rc > 0){
				printf("result %s\n", rx_message);
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
	return 0;
}
