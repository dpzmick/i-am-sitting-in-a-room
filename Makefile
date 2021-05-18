all: room level

room: room.c Makefile
	gcc -O3 room.c -o room -ljack -lsndfile -Werror -Wall -Wextra -Wconversion -fno-strict-aliasing -ffast-math -Wno-unused-parameter -g

level: level.c Makefile
	gcc -O3 level.c -o level -ljack -Werror -Wall -Wextra -Wconversion -fno-strict-aliasing -ffast-math -Wno-unused-parameter -g
