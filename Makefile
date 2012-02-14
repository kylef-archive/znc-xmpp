CXX := g++
CXXFLAGS := -I/usr/include/libxml2 -fPIC
LIBS := -lxml2

all:
	$(CXX) $(CXXFLAGS) -c src/Stanza.cpp -o src/Stanza.o
	$(CXX) $(CXXFLAGS) -c src/Client.cpp -o src/Client.o
	$(CXX) $(CXXFLAGS) -c src/Listener.cpp -o src/Listener.o
	$(CXX) $(CXXFLAGS) -c src/xmpp.cpp -o src/xmpp.o
	$(CXX) $(LIBS) -o xmpp.so src/xmpp.o src/Stanza.o src/Client.o src/Listener.o -shared

clean:
	rm src/*.o
	*.so
