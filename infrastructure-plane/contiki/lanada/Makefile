all: udp-client udp-server
CONTIKI=../

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

#TARGET_MEMORY_MODEL = large
#CFLAGS += -mcode-region=any -mdata-region=any

#CFLAGS += -mmemory-model=medium \
				      -ffunction-sections -fdata-sections \
							     -mcode-region=any -mdata-region=any
#LDFLAGS += -mmemory-model=medium \
					      -Wl,-gc-sections \
								     -mcode-region=any -mdata-region=any

ifdef WITH_COMPOWER
APPS+=powertrace
CFLAGS+= -DCONTIKIMAC_CONF_COMPOWER=1 -DWITH_COMPOWER=1 -DQUEUEBUF_CONF_NUM=4
endif

ifdef SERVER_REPLY
CFLAGS+=-DSERVER_REPLY=$(SERVER_REPLY)
endif
ifdef PERIOD
CFLAGS+=-DPERIOD=$(PERIOD)
endif

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include

