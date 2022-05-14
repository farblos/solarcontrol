#
# GNUmakefile - arduino project makefile.
#

# system configuration.  Arduino.mk does not seem to get the
# Arduino version right.
ARDUINO_DIR :=		/usr/share/arduino
ARDUINO_VERSION :=	$(shell cat $(ARDUINO_DIR)/lib/version.txt | \
				sed 's/^[0-9]://;s/\.\([0-9]\)\([^0-9]\)/.0\1\2/g;s/\.//g;s/\+.*//')
AVR_TOOLS_PATH :=	/bin
AVRDUDE_CONF :=		/etc/avrdude.conf

# board configuration
BOARD_TAG :=		nano			# arduino nano ...
BOARD_SUB :=		atmega328old		# ... with old boot loader

# sketch configuration.  Must specify these explicitly since the
# automatic library detection of Arduino.mk does not work
# reliably.
USER_LIB_PATH :=	$(realpath ../../libraries)
ARDUINO_LIBS :=					\
			Adafruit_BusIO		\
			DallasTemperature	\
			NewliquidCrystal	\
			OneWire			\
			RTClib			\
			SD			\
			SPI			\
			Wire

include /usr/share/arduino/Arduino.mk
