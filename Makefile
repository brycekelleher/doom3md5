CXX = clang
CXXFLAGS = -ggdb
LIBS = -lm -lGL -lglut

.PHONY: clean build run

build: gldoom3md5

clean:
	rm -f gldoom3md5 gldoom3md5.o

run: gldoom3md5
	find md5/monsters/imp/*.md5mesh md5/monsters/imp/*.md5anim | xargs ./gldoom3md5

gldoom3md5: gldoom3md5.o
	gcc $< -o $@ $(LIBS)

gldoom3md5.o: gldoom3md5.cpp
	gcc -c $< -o $@ $(CXXFLAGS)
