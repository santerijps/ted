CC 		:= gcc
INC		:= -I../clib
WARN	:= -Wall -Wextra
OPT		:= -s -O3 -fno-ident -fno-asynchronous-unwind-tables -fdata-sections -ffunction-sections -fipa-pta -Wl,--gc-sections -Wl,-O1 -Wl,--as-needed -Wl,--strip-all

debug:
	$(CC) $(INC) $(WARN) -Og -o ted.debug.exe main.c

release:
	$(CC) $(INC) $(WARN) $(OPT) -o ted.exe main.c
