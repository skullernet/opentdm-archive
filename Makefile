default: gamei386.so

gamei386.so: *.c *.h
	$(CC) -Wall -Os -g *.c -o gamei386.so -shared

