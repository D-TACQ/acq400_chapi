
CXXFLAGS+=-std=c++17
CXXFLAGS+=-O2

CXXFLAGS+=-I../../acq400_chapi
LDLIBS+=../../acq400_chapi/acq400_chapi.o -lpopt

all: apps

APPS=acq400_upload acq400_spad_frame_decoder

apps: $(APPS)

install_user: apps
	cp $(APPS) $(HOME)/bin


clean:
	rm -f $(APPS)
