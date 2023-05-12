PROJECT_NAME = sss
BOARD = stm32f4_disco

all: build

build:
	west build -b $(BOARD) -d build

clean:
	west build -t clean

flash:
	west flash

