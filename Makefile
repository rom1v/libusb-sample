.PHONY: clean

OUT = sample
OBJ = main.o
CFLAGS = -std=c11 -Wall -g
LIBS = -lusb-1.0 -lpthread

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) $(LIBS) -o $(OUT) $(OBJ)

$(OBJ):

clean:
	rm -f $(OBJ) $(OUT)
