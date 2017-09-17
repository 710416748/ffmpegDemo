cc += gcc -g
LIBS += -L/usr/local/lib -lavcodec  -lavdevice  -lavfilter  -lavformat \
        -lavutil  -lswresample  -lswscale -lpthread

targe : demo
	$(cc) demo.o -o demo $(LIBS)

demo : demo.c
	$(CC) -c demo.c
