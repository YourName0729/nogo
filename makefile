all:
	g++ -std=c++17 -O3 -g -Wall -fmessage-length=0 -o nogo nogo.cpp
clean:
	rm -r nogo gogui-twogtp-*