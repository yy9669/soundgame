
#from https://stackoverflow.com/questions/714100/os-detecting-makefile
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	OPUSENC = ../../nest-libs/linux/opus-tools/bin/opusenc
else
	OPUSENC = ../../nest-libs/macos/opus-tools/bin/opusenc
endif

../dist/death.opus : death.wav
	$(OPUSENC) --vbr --bitrate 128 death.wav ../dist/death.opus

# ../dist/warn.opus : warn.wav
# 	$(OPUSENC) --vbr --bitrate 128 warn.wav ../dist/warn.opus