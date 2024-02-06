
#CXXFLAGS+=-std=c++11
CXXFLAGS+=-std=c++17
#CXXFLAGS+=-std=c++20

CXXFLAGS+=-O2

all: lib apps

lib: acq400_chapi.o 
	
	
APPS=acq400_chapi_siteclient_test acq400_chapi_acq400_test acq400_stream acq400_stream_awg acq400_chapi_soft_trigger	

apps: $(APPS)


acq400_chapi_siteclient_test: acq400_chapi_siteclient_test.cpp acq400_chapi.o

acq400_chapi_soft_trigger: acq400_chapi_soft_trigger.cpp acq400_chapi.o

acq400_chapi_acq400_test: acq400_chapi_acq400_test.cpp acq400_chapi.o

acq400_stream: acq400_stream.cpp acq400_chapi.o

acq400_stream_awg: acq400_stream_awg.cpp acq400_chapi.o

acq400_chapi.o: acq400_chapi.cpp

clean:
	rm -f  $(APPS) *.o
