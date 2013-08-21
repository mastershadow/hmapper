all:
	gcc main.c -ltiff -lgeotiff -o hmapper

clean:
	rm hmapper