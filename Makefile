default: gamei386.so

gamei386.so: *.c *.h
	$(CC) -Wall -fPIC -Os -g *.c -o gamei386.so -shared -lm

