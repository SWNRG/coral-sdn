CONTIKI_PROJECT = coral
all: $(CONTIKI_PROJECT)

TARGET=z1

CONTIKI = ../../../contiki
CONTIKI_WITH_RIME = 1
#CONTIKI_TARGET_SOURCEFILES += dht22.c
#PROJECT_SOURCEFILES = addr.c packet.c neighbor.c 
#flowtable.c packet-buffer.c address.c neighbor-table.c packet-handler.c node-conf.c packet-creator.c
#TARGETDIRS += ../targets
CFLAGS += -DPROJECT_CONF_H=\"net/coral-sdn/coral_conf.h\"

include $(CONTIKI)/Makefile.include
