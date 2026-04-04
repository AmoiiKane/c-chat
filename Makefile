# Detect OS
ifeq ($(OS), Windows_NT)
  CC     = gcc
  CFLAGS = -Wall -Wextra -g
  LIBS   = -lws2_32
  EXT    = .exe
  RM     = del /Q /F
  MKDIR  = mkdir
else
  CC             = gcc
  CFLAGS_DEBUG   = -Wall -Wextra -Werror -pthread -g -O0
  CFLAGS_RELEASE = -Wall -Wextra -Werror -pthread -O2 -DNDEBUG
  CFLAGS        ?= $(CFLAGS_DEBUG)
  LIBS           =
  EXT            =
  RM             = rm -rf
  MKDIR          = mkdir -p
  GUI_CFLAGS     = $(shell pkg-config --cflags webkit2gtk-4.1 gtk+-3.0 2>/dev/null)
  GUI_LIBS       = $(shell pkg-config --libs   webkit2gtk-4.1 gtk+-3.0 2>/dev/null)
endif

SRC = src
BIN = bin

all: $(BIN)/server$(EXT) $(BIN)/client$(EXT) $(BIN)/gui_client$(EXT)

release: CFLAGS = $(CFLAGS_RELEASE)
release: all

$(BIN):
ifeq ($(OS), Windows_NT)
	if not exist $(BIN) $(MKDIR) $(BIN)
else
	$(MKDIR) $(BIN)
endif

$(BIN)/server$(EXT): $(SRC)/server.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

$(BIN)/client$(EXT): $(SRC)/client.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

$(BIN)/gui_client$(EXT): $(SRC)/gui_client.c | $(BIN)
	$(CC) $(CFLAGS) $(GUI_CFLAGS) $< -o $@ $(GUI_LIBS)

clean:
ifeq ($(OS), Windows_NT)
	$(RM) $(BIN)\server.exe $(BIN)\client.exe $(BIN)\gui_client.exe
else
	$(RM) $(BIN)
endif

re: clean all

.PHONY: all clean re release
