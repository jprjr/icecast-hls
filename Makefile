.PHONY: all clean

PKGCONFIG=pkg-config
CFLAGS = -Wall -Wextra -g -O2 -fPIC -pthread
LDFLAGS = -pthread

SOURCES = \
	src/avcodec_utils.c \
	src/avframe_utils.c \
	src/avpacket_utils.c \
	src/ini.c \
	src/map.c \
	src/membuf.c \
	src/main.c \
	src/miniflac.c \
	src/minifmp4.c \
	src/ts.c \
	src/tflac.c \
	src/adts_mux.c \
	src/codecs.c \
	src/decoder.c \
	src/decoder_plugin.c \
	src/decoder_plugin_auto.c \
	src/decoder_plugin_avcodec.c \
	src/decoder_plugin_miniflac.c \
	src/decoder_plugin_passthrough.c \
	src/demuxer.c \
	src/demuxer_plugin.c \
	src/demuxer_plugin_auto.c \
	src/demuxer_plugin_avformat.c \
	src/demuxer_plugin_flac.c \
	src/demuxer_plugin_ogg.c \
	src/destination.c \
	src/destinationlist.c \
	src/destination_sync.c \
	src/encoder.c \
	src/encoder_plugin.c \
	src/encoder_plugin_avcodec.c \
	src/encoder_plugin_exhale.c \
	src/encoder_plugin_fdk_aac.c \
	src/encoder_plugin_opus.c \
	src/encoder_plugin_passthrough.c \
	src/filter.c \
	src/filter_plugin.c \
	src/filter_plugin_avfilter.c \
	src/filter_plugin_passthrough.c \
	src/frame.c \
	src/hls.c \
	src/ich_time.c \
	src/id3.c \
	src/input.c \
	src/input_plugin.c \
	src/input_plugin_curl.c \
	src/input_plugin_file.c \
	src/input_plugin_stdin.c \
	src/logger.c \
	src/muxer.c \
	src/muxer_plugin.c \
	src/muxer_plugin_adts.c \
	src/muxer_plugin_flac.c \
	src/muxer_plugin_fmp4.c \
	src/muxer_plugin_ogg.c \
	src/muxer_plugin_ogg_flac.c \
	src/muxer_plugin_ogg_opus.c \
	src/muxer_plugin_packedaudio.c \
	src/muxer_plugin_passthrough.c \
	src/muxer_plugin_passthrough.c \
	src/muxer_plugin_ts.c \
	src/output.c \
	src/output_plugin.c \
	src/output_plugin_curl.c \
	src/output_plugin_file.c \
	src/output_plugin_icecast.c \
	src/output_plugin_stdout.c \
	src/output_plugin_folder.c \
	src/packet.c \
	src/samplefmt.c \
	src/segment.c \
	src/socket.c \
	src/source.c \
	src/sourcelist.c \
	src/source_sync.c \
	src/str.c \
	src/strbuf.c \
	src/tag.c \
	src/tagmap.c \
	src/tagmap_default.c \
	src/thread.c \
	src/version.c

OBJS = $(SOURCES:%.c=%.o)

REQUIRED_OBJS = \
	src/ini.o \
	src/map.o \
	src/main.o \
	src/membuf.o \
	src/miniflac.o \
	src/minifmp4.o \
	src/codecs.o \
	src/adts_mux.o \
	src/tflac.o \
	src/ts.o \
	src/decoder.o \
	src/decoder_plugin.o \
	src/decoder_plugin_auto.o \
	src/decoder_plugin_miniflac.o \
	src/decoder_plugin_passthrough.o \
	src/demuxer.o \
	src/demuxer_plugin.o \
	src/demuxer_plugin_auto.o \
	src/demuxer_plugin_flac.o \
	src/demuxer_plugin_ogg.o \
	src/destination.o \
	src/destinationlist.o \
	src/destination_sync.o \
	src/encoder.o \
	src/encoder_plugin.o \
	src/encoder_plugin_tflac.o \
	src/encoder_plugin_passthrough.o \
	src/filter.o \
	src/filter_plugin.o \
	src/filter_plugin_passthrough.o \
	src/frame.o \
	src/hls.o \
	src/ich_time.o \
	src/id3.o \
	src/input.o \
	src/input_plugin.o \
	src/input_plugin_file.o \
	src/input_plugin_stdin.o \
	src/logger.o \
	src/muxer.o \
	src/muxer_plugin.o \
	src/muxer_plugin_adts.o \
	src/muxer_plugin_flac.o \
	src/muxer_plugin_fmp4.o \
	src/muxer_plugin_ogg.o \
	src/muxer_plugin_ogg_flac.o \
	src/muxer_plugin_ogg_opus.o \
	src/muxer_plugin_packedaudio.o \
	src/muxer_plugin_passthrough.o \
	src/muxer_plugin_ts.o \
	src/output.o \
	src/output_plugin.o \
	src/output_plugin_file.o \
	src/output_plugin_icecast.o \
	src/output_plugin_stdout.o \
	src/output_plugin_folder.o \
	src/packet.o \
	src/samplefmt.o \
	src/segment.o \
	src/socket.o \
	src/source.o \
	src/sourcelist.o \
	src/source_sync.o \
	src/str.o \
	src/strbuf.o \
	src/tag.o \
	src/tagmap.o \
	src/tagmap_default.o \
	src/thread.o \
	src/version.o

PKGCONFIG_LIBS =
# libcurl fdk-aac opus libavformat libavfilter libavutil libavcodec

CFLAGS_EXHALE =
LDFLAGS_EXHALE = -lexhale

ENCODER_PLUGIN_CFLAGS =
FILTER_PLUGIN_CFLAGS =

# temp because I just want to build everything while testing
ENABLE_FDK_AAC=1
ENABLE_EXHALE=1
ENABLE_OPUS=1
ENABLE_AVFILTER=1
ENABLE_AVFORMAT=1
ENABLE_AVCODEC=1
ENABLE_CURL=1

ifeq ($(ENABLE_AVFILTER),1)
FILTER_PLUGIN_CFLAGS += -DFILTER_PLUGIN_AVFILTER=1
REQUIRED_OBJS += src/filter_plugin_avfilter.o
AVFRAME_REQUIRED=1
AVUTIL_REQUIRED=1
AVFILTER_REQUIRED=1
endif

ifeq ($(ENABLE_AVFORMAT),1)
DEMUXER_PLUGIN_CFLAGS += -DDEMUXER_PLUGIN_AVFORMAT=1
REQUIRED_OBJS += src/demuxer_plugin_avformat.o
AVPACKET_REQUIRED=1
AVCODEC_REQUIRED=1
AVUTIL_REQUIRED=1
AVFORMAT_REQUIRED=1
endif

ifeq ($(ENABLE_AVCODEC),1)
ENCODER_PLUGIN_CFLAGS += -DENCODER_PLUGIN_AVCODEC=1
DECODER_PLUGIN_CFLAGS += -DDECODER_PLUGIN_AVCODEC=1
REQUIRED_OBJS += src/encoder_plugin_avcodec.o
REQUIRED_OBJS += src/decoder_plugin_avcodec.o
AVFRAME_REQUIRED=1
AVPACKET_REQUIRED=1
AVUTIL_REQUIRED=1
AVCODEC_REQUIRED=1
AVFORMAT_REQUIRED=1
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
OUTPUT_PLUGIN_CFLAGS += -DOUTPUT_PLUGIN_CURL=1
REQUIRED_OBJS += src/input_plugin_curl.o
REQUIRED_OBJS += src/output_plugin_curl.o
CURL_REQUIRED=1
endif

ifeq ($(ENABLE_OPUS),1)
ENCODER_PLUGIN_CFLAGS += -DENCODER_PLUGIN_OPUS=1
REQUIRED_OBJS += src/encoder_plugin_opus.o
OPUS_REQUIRED=1
endif

ifeq ($(EXHALE_REQUIRED),1)
LDFLAGS += $(LDFLAGS_EXHALE)
endif

ifeq ($(FDK_AAC_REQUIRED),1)
PKGCONFIG_LIBS += fdk-aac
endif

ifeq ($(OPUS_REQUIRED),1)
PKGCONFIG_LIBS += opus
endif

ifeq ($(AVFORMAT_REQUIRED),1)
PKGCONFIG_LIBS += libavformat
endif

ifeq ($(AVFILTER_REQUIRED),1)
PKGCONFIG_LIBS += libavfilter
endif

ifeq ($(AVCODEC_REQUIRED),1)
REQUIRED_OBJS += src/avcodec_utils.o
PKGCONFIG_LIBS += libavcodec
endif

ifeq ($(AVUTIL_REQUIRED),1)
PKGCONFIG_LIBS += libavutil
endif

ifeq ($(AVFRAME_REQUIRED),1)
REQUIRED_OBJS += src/avframe_utils.o
endif

ifeq ($(AVPACKET_REQUIRED),1)
REQUIRED_OBJS += src/avpacket_utils.o
endif

ifeq ($(CURL_REQUIRED),1)
PKGCONFIG_LIBS += libcurl
endif

all: icecast-hls

clean:
	rm -f $(OBJS) icecast-hls

icecast-hls: $(REQUIRED_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(shell $(PKGCONFIG) --libs $(PKGCONFIG_LIBS))

src/decoder_plugin.o: src/decoder_plugin.c
	$(CC) $(CFLAGS) $(DECODER_PLUGIN_CFLAGS) -c -o $@ $<

src/demuxer_plugin.o: src/demuxer_plugin.c
	$(CC) $(CFLAGS) $(DEMUXER_PLUGIN_CFLAGS) -c -o $@ $<

src/encoder_plugin.o: src/encoder_plugin.c
	$(CC) $(CFLAGS) $(ENCODER_PLUGIN_CFLAGS) -c -o $@ $<

src/filter_plugin.o: src/filter_plugin.c
	$(CC) $(CFLAGS) $(FILTER_PLUGIN_CFLAGS) -c -o $@ $<

src/input_plugin.o: src/input_plugin.c
	$(CC) $(CFLAGS) $(INPUT_PLUGIN_CFLAGS) -c -o $@ $<

src/output_plugin.o: src/output_plugin.c
	$(CC) $(CFLAGS) $(OUTPUT_PLUGIN_CFLAGS) -c -o $@ $<

src/avcodec_utils.o: src/avcodec_utils.c src/ffmpeg-versions.h
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags libavcodec) -c -o $@ $<

src/avpacket_utils.o: src/avpacket_utils.c src/ffmpeg-versions.h
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags libavcodec) -c -o $@ $<

src/avframe_utils.o: src/avframe_utils.c src/ffmpeg-versions.h
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags libavutil) -c -o $@ $<

src/filter_plugin_avfilter.o: src/filter_plugin_avfilter.c src/ffmpeg-versions.h
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags libavfilter libavutil) -c -o $@ $<

src/encoder_plugin_exhale.o: src/encoder_plugin_exhale.c
	$(CC) $(CFLAGS) $(CFLAGS_EXHALE) -c -o $@ $<

src/encoder_plugin_opus.o: src/encoder_plugin_opus.c
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags opus) -c -o $@ $<

src/decoder_plugin_avcodec.o: src/decoder_plugin_avcodec.c src/ffmpeg-versions.h
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags libavformat libavcodec libavutil) -c -o $@ $<

src/encoder_plugin_avcodec.o: src/encoder_plugin_avcodec.c src/ffmpeg-versions.h
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags libavcodec libavutil ) -c -o $@ $<

src/encoder_plugin_fdk_aac.o: src/encoder_plugin_fdk_aac.c
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags fdk-aac) -c -o $@ $<

src/input_plugin_curl.o: src/input_plugin_curl.c
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags libcurl) -c -o $@ $<

src/output_plugin_curl.o: src/output_plugin_curl.c
	$(CC) $(CFLAGS) $(CFLAGS_CURL) -c -o $@ $<

src/decoder_plugin_miniflac.o: src/decoder_plugin_miniflac.c src/miniflac.h src/base64decode.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/encoder_plugin_tflac.o: src/encoder_plugin_tflac.c src/tflac.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/demuxer_plugin_avformat.o: src/demuxer_plugin_avformat.c src/ffmpeg-versions.h
	$(CC) $(CFLAGS) $(shell $(PKGCONFIG) --cflags libavformat libavcodec libavutil) -c -o $@ $<

src/minifmp4.o: src/minifmp4.c src/minifmp4.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/thread.o: src/thread.c src/thread.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/miniflac.o: src/miniflac.c src/miniflac.h
	$(CC) $(CFLAGS) -c -o $@ $<

