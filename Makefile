clientchans.so : clientchans.cpp
	znc-buildmod clientchans.cpp

install: clientchans.so
	install clientchans.so /usr/lib/znc/
