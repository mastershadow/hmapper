all:
	gcc hmapper.c -ltiff -lgeotiff -o hmapper

clean:
	rm hmapper
