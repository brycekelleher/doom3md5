CC = clang
CFLAGS = -ggdb
LIBS = -lm -lGL -lglut

.PHONY: clean build run

build: gldoom3md5

clean:
	rm -f gldoom3md5 gldoom3md5.o

run: turtle-bin turtle.sh
	./turtle.sh

gldoom3md5: gldoom3md5.o
	gcc $< -o $@ $(LIBS)

gldoom3md5.o: gldoom3md5.c
	gcc -c $< -o $@ $(CFLAGS)
