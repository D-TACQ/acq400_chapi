
#CXXFLAGS+=-std=c++11
CXXFLAGS+=-std=c++17

all: acq400_chapi_siteclient_test acq400_chapi_acq400_test


acq400_chapi_siteclient_test: acq400_chapi_siteclient_test.cpp acq400_chapi.cpp

acq400_chapi_acq400_test: acq400_chapi_acq400_test.cpp acq400_chapi.cpp

clean:
	rm -f  acq400_chapi_siteclient_test *.o