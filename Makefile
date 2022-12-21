.PHONY: all clean

CFLAGS = -Wall -Wextra -g -O3
LDFLAGS = 

SOURCES = \
	src/avframe_utils.c \
	src/avpacket_utils.c \
	src/ini.c \
	src/map.c \
	src/membuf.c \
	src/main.c \
	src/miniflac.c \
	src/minifmp4.c \
	src/codecs.c \
	src/decoder.c \
	src/decoder_plugin.c \
	src/decoder_plugin_miniflac.c \
	src/destination.c \
	src/destinationlist.c \
	src/destination_sync.c \
	src/encoder.c \
	src/encoder_plugin.c \
	src/encoder_plugin_avcodec.c \
	src/encoder_plugin_exhale.c \
	src/encoder_plugin_fdk_aac.c \
	src/filter.c \
	src/filter_plugin.c \
	src/filter_plugin_avfilter.c \
	src/filter_plugin_buffer.c \
	src/filter_plugin_passthrough.c \
	src/frame.c \
	src/hls.c \
	src/ich_time.c \
	src/id3.c \
	src/input.c \
	src/input_plugin.c \
	src/input_plugin_file.c \
	src/input_plugin_stdin.c \
	src/muxer.c \
	src/muxer_plugin.c \
	src/muxer_plugin_fmp4.c \
	src/muxer_plugin_packedaudio.c \
	src/output.c \
	src/output_plugin.c \
	src/output_plugin_file.c \
	src/output_plugin_stdout.c \
	src/output_plugin_folder.c \
	src/packet.c \
	src/samplefmt.c \
	src/segment.c \
	src/source.c \
	src/sourcelist.c \
	src/source_sync.c \
	src/str.c \
	src/strbuf.c \
	src/tag.c \
	src/tagmap.c \
	src/tagmap_default.c \
	src/thread.c

OBJS = $(SOURCES:%.c=%.o)

REQUIRED_OBJS = \
	src/ini.o \
	src/map.o \
	src/main.o \
	src/membuf.o \
	src/miniflac.o \
	src/minifmp4.o \
	src/codecs.c \
	src/decoder.o \
	src/decoder_plugin.o \
	src/decoder_plugin_miniflac.o \
	src/destination.o \
	src/destinationlist.o \
	src/destination_sync.o \
	src/encoder.o \
	src/encoder_plugin.o \
	src/filter.o \
	src/filter_plugin.o \
	src/filter_plugin_buffer.o \
	src/filter_plugin_passthrough.o \
	src/frame.o \
	src/hls.o \
	src/ich_time.o \
	src/id3.o \
	src/input.o \
	src/input_plugin.o \
	src/input_plugin_file.o \
	src/input_plugin_stdin.o \
	src/muxer.o \
	src/muxer_plugin.o \
	src/muxer_plugin_fmp4.o \
	src/muxer_plugin_packedaudio.o \
	src/output.o \
	src/output_plugin.o \
	src/output_plugin_file.o \
	src/output_plugin_stdout.o \
	src/output_plugin_folder.o \
	src/packet.o \
	src/samplefmt.o \
	src/segment.c \
	src/source.o \
	src/sourcelist.o \
	src/source_sync.o \
	src/str.o \
	src/strbuf.o \
	src/tag.o \
	src/tagmap.o \
	src/tagmap_default.o \
	src/thread.o

CFLAGS_EXHALE =
LDFLAGS_EXHALE = -lexhale

CFLAGS_CURL = $(shell pkg-config --cflags libcurl)
LDFLAGS_CURL = $(shell pkg-config --libs libcurl)

CFLAGS_FDK_AAC = $(shell pkg-config --cflags fdk-aac)
LDFLAGS_FDK_AAC = $(shell pkg-config --libs fdk-aac)

CFLAGS_AVFILTER = $(shell pkg-config --cflags libavfilter)
LDFLAGS_AVFILTER = $(shell pkg-config --libs libavfilter)

CFLAGS_AVUTIL = $(shell pkg-config --cflags libavutil)
LDFLAGS_AVUTIL = $(shell pkg-config --libs libavutil)

CFLAGS_AVCODEC = $(shell pkg-config --cflags libavcodec)
LDFLAGS_AVCODEC = $(shell pkg-config --libs libavcodec)

ENCODER_PLUGIN_CFLAGS =
FILTER_PLUGIN_CFLAGS =

# temp because I just want to build everything while testing
ENABLE_FDK_AAC=1
ENABLE_EXHALE=1
ENABLE_AVFILTER=1
ENABLE_AVCODEC=1
ENABLE_CURL=1

ifeq ($(ENABLE_AVFILTER),1)
FILTER_PLUGIN_CFLAGS += -DFILTER_PLUGIN_AVFILTER=1
REQUIRED_OBJS += src/filter_plugin_avfilter.o
AVFRAME_REQUIRED=1
AVUTIL_REQUIRED=1
AVFILTER_REQUIRED=1
endif

ifeq ($(ENABLE_AVCODEC),1)
ENCODER_PLUGIN_CFLAGS += -DENCODER_PLUGIN_AVCODEC=1
REQUIRED_OBJS += src/encoder_plugin_avcodec.o
AVFRAME_REQUIRED=1
AVPACKET_REQUIRED=1
AVUTIL_REQUIRED=1
AVCODEC_REQUIRED=1
endif

ifeq ($(ENABLE_EXHALE),1)
ENCODER_PLUGIN_CFLAGS += -DENCODER_PLUGIN_EXHALE=1
REQUIRED_OBJS += src/encoder_plugin_exhale.o
EXHALE_REQUIRED=1
endif

ifeq ($(ENABLE_FDK_AAC),1)
ENCODER_PLUGIN_CFLAGS += -DENCODER_PLUGIN_FDK_AAC=1
REQUIRED_OBJS += src/encoder_plugin_fdk_aac.o
FDK_AAC_REQUIRED=1
endif

ifeq ($(ENABLE_CURL),1)
INPUT_PLUGIN_CFLAGS += -DINPUT_PLUGIN_CURL=1
#OUTPUT_PLUGIN_CFLAGS += -DOUTPUT_PLUGIN_CURL=1
REQUIRED_OBJS += src/input_plugin_curl.o
#REQUIRED_OBJS += src/output_plugin_curl.o
CURL_REQUIRED=1
endif

ifeq ($(EXHALE_REQUIRED),1)
LDFLAGS += $(LDFLAGS_EXHALE)
endif

ifeq ($(FDK_AAC_REQUIRED),1)
LDFLAGS += $(LDFLAGS_FDK_AAC)
endif

ifeq ($(AVFILTER_REQUIRED),1)
LDFLAGS += $(LDFLAGS_AVFILTER)
endif

ifeq ($(AVUTIL_REQUIRED),1)
LDFLAGS += $(LDFLAGS_AVUTIL)
endif

ifeq ($(AVCODEC_REQUIRED),1)
LDFLAGS += $(LDFLAGS_AVCODEC)
endif

ifeq ($(AVFRAME_REQUIRED),1)
REQUIRED_OBJS += src/avframe_utils.o
endif

ifeq ($(AVPACKET_REQUIRED),1)
REQUIRED_OBJS += src/avpacket_utils.o
endif

ifeq ($(CURL_REQUIRED),1)
LDFLAGS += $(LDFLAGS_CURL)
endif

all: icecast-hls

clean:
	rm -f $(OBJS) icecast-hls

icecast-hls: $(REQUIRED_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

src/encoder_plugin.o: src/encoder_plugin.c
	$(CC) $(CFLAGS) $(ENCODER_PLUGIN_CFLAGS) -c -o $@ $<

src/filter_plugin.o: src/filter_plugin.c
	$(CC) $(CFLAGS) $(FILTER_PLUGIN_CFLAGS) -c -o $@ $<

src/input_plugin.o: src/input_plugin.c
	$(CC) $(CFLAGS) $(INPUT_PLUGIN_CFLAGS) -c -o $@ $<

src/avpacket_utils.o: src/avpacket_utils.c
	$(CC) $(CFLAGS) $(CFLAGS_AVCODEC) -c -o $@ $<

src/avframe_utils.o: src/avframe_utils.c
	$(CC) $(CFLAGS) $(CFLAGS_AVUTIL) -c -o $@ $<

src/filter_plugin_avfilter.o: src/filter_plugin_avfilter.c
	$(CC) $(CFLAGS) $(CFLAGS_AVFILTER) $(CFLAGS_AVUTIL) -c -o $@ $<

src/encoder_plugin_exhale.o: src/encoder_plugin_exhale.c
	$(CC) $(CFLAGS) $(CFLAGS_EXHALE) -c -o $@ $<

src/encoder_plugin_avcodec.o: src/encoder_plugin_avcodec.c
	$(CC) $(CFLAGS) $(CFLAGS_AVCODEC) $(CFLAGS_AVUTIL) -c -o $@ $<

src/encoder_plugin_fdk_aac.o: src/encoder_plugin_fdk_aac.c
	$(CC) $(CFLAGS) $(CFLAGS_FDK_AAC) -c -o $@ $<

src/input_plugin_curl.o: src/input_plugin_curl.c
	$(CC) $(CFLAGS) $(CFLAGS_CURL) -c -o $@ $<

src/decoder_plugin_miniflac.o: src/decoder_plugin_miniflac.c src/miniflac.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/minifmp4.o: src/minifmp4.c src/minifmp4.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/thread.o: src/thread.c src/thread.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/miniflac.o: src/miniflac.c src/miniflac.h
	$(CC) $(CFLAGS) -c -o $@ $<

dump:
	echo $(OBJS)
