all: shellcraftm
kbhit.o: kbhit.c
	gcc -c kbhit.c
shellcraftm: shellcraftm.c kbhit.o
	gcc shellcraftm.c kbhit.o -o shellcraftm -lm
