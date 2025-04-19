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


enum SetGet {
	sgGet, sgSet
};

enum SetGet decode(std::string &site, std::string &keyval, const char* user_command)
{
	char the_command[1024];
	char mode[4];			// set or get
	char scode[3];

	if (sscanf(user_command, "%3s.site %2s %1024[^\n]", mode, scode, the_command) == 3){
		printf("success: \"%s\" \"%s\" \"%s\"\n", mode, scode, the_command);
		site = std::string(scode);
		keyval = std::string(the_command);
		if (strcmp(mode, "set") == 0){
			return sgSet;
		}else{
			return sgGet;
		}
	}else{
		site = std::string("0");
		keyval = std::string(user_command);
	}
	return sgGet;
}

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
		uut.get(model, key, "MODEL");
		std::cout << "s" << key << " MODEL:" << model << std::endl;
	}

	std::string rx_message;
	std::string keyval;
	std::string site;
	int rc;

	if (argc > 2){
		for (int ic = 3; ic < argc; ++ic){
			switch(decode(site, keyval, argv[ic])){
			case sgGet:
				rc = uut.get(rx_message, site, keyval.c_str());
				if (rc >= 0){
					std::cout << "result " << rx_message << std::endl;
				}else{
					fprintf(stderr, "ERROR: sr returned %d\n", rc);
				}
				break;
			case sgSet:
				rc = uut.set(rx_message, site, keyval.c_str());
				if (rc >= 0){
					std::cout << "result " << rx_message << std::endl;
				}else{
					fprintf(stderr, "ERROR: sr returned %d\n", rc);
				}
			}

		}
	}else{
		std::string tx_message;
		int ic = 0;

		printf("ready %d>", ic); fflush(stdout);
		while(std::getline(std::cin, tx_message)){
			switch(decode(site, keyval, tx_message.c_str())){
			case sgGet:
				rc = uut.get(rx_message, site, keyval.c_str());
				if (rc >= 0){
					std::cout << "result " << rx_message << std::endl;
				}else{
					fprintf(stderr, "ERROR: sr returned %d\n", rc);
				}
				break;
			case sgSet:
				rc = uut.set(rx_message, site, keyval.c_str());
				if (rc >= 0){
					std::cout << "result " << rx_message << std::endl;
				}else{
					fprintf(stderr, "ERROR: sr returned %d\n", rc);
				}
			}
			printf("ready %d>", ++ic); fflush(stdout);
			rx_message.clear();
		}
	}

	return 0;
}

