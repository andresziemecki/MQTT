mqtt: mqtt.cpp
	g++ -std=c++11 -pthread -Wall mqtt.cpp -o mqtt
# This one is for mutiple files
mf: broker.cpp broker.hpp client.cpp client.hpp cola.h main.cpp simclient.cpp simclient.hpp
	g++ -std=c++11 -pthread -Wall broker.cpp client.cpp main.cpp simclient.cpp -o mqtt
