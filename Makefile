PROJECT_NAME = sss
BOARD = stm32f4_disco
OVERLAY_FILE = fdo2.dtso
DTS_DIR = dts
DTC_FLAGS = -O dtb -o $(DTS_DIR)/$(OVERLAY_FILE:.dtso=.dtbo)

all: build

build:
	west build -b $(BOARD) -d build -- -DOVERLAY_FILE=$(DTS_DIR)/$(OVERLAY_FILE)

clean:
	west build -t clean

compile_overlay:
	@mkdir -p $(DTS_DIR)
	dtc $(DTC_FLAGS) $(DTS_DIR)/$(OVERLAY_FILE)

flash:
	west flash

