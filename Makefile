.PHONY: all

all: WTFclient.c WTFserver.c
	gcc -pthread -lssl -lcrypto -o WTF WTFclient.c
	gcc -pthread -lssl -lcrypto -o WTFserver WTFserver.c

.PHONY: clean

clean: 
	rm WTF	
	rm WTFserver

testSomething:
	./Client/WTF configure 127.0.0.1 9123