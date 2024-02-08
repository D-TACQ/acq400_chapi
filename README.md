# acq400_chapi : acq400 series C language Host API

- network host api, similar to but lighter than HAPI, the python Host API

*** NB: this is still very skeletal ***


# boost is no longer needed
#sudo dnf install boost-devel
# we do need popt
## ubuntu
sudo apt-get install libpopt-dev

# Install recommendation:
# works on all known Linux including WSL
# native Windows port would need a socket abstraction layer
```bash
mkdir PROJECTS
cd PROJECTS; git clone https://D-TACQ/acq400_chapi
cd acq400_chapi
make
```


 # Example
```
[pgm@hoy5 acq400_chapi]$ ./acq400_chapi_siteclient_test acq2106_320 4220
connect to acq2106_320:4220
ready 0>ssb
64
ready 1>software_version
acq400-411-20210902101420
ready 2>MODEL
acq2106
ready 3>HN 
acq2106_320
ready 4>SIG:CLK_MB:FREQ
SIG:CLK_MB:FREQ 29999776.000
ready 6>help TRAN* 
TRANSIENT
TRANSIENT:DELAYMS
TRANSIENT:OSAM
TRANSIENT:POST
TRANSIENT:PRE
TRANSIENT:REPEAT
TRANSIENT:SET_ABORT
TRANSIENT:SET_ARM
TRANSIENT:SOFT_TRIGGER
TRANS_ACT:FIND_EV:CUR
TRANS_ACT:FIND_EV:NBU
TRANS_ACT:FIND_EV:STA
TRANS_ACT:POST
TRANS_ACT:POST:MDSPUTCH
TRANS_ACT:PRE
TRANS_ACT:STATE
TRANS_ACT:STATE_NOT_IDLE
TRANS_ACT:TOTSAM
```

 # Streaming .. emulates nc, use this as a base to build from

```
./acq400_stream UUT 4210 | pv > /dev/null
```

 # AWG Stream
```
while [ 1 ]; do cat DATAFILE; done | ./acq400_stream_awg UUT
```


