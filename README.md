# icecast-hls

This is a program that accepts an Icecast stream and re-encodes it
into 1 or more HLS streams.

It's main use is with chained Ogg streams with FLAC audio, which
allows for in-band metadata, images, etc. It can also work with any
other format + codec supported by libavformat/libavcodec - though using
a lossless source like FLAC is preferred.

Everything is implemented as a plugin - decoding can be done
via different plugins, encoding done via plugins, muxing done
via plugins.

The HLS streams can use either packed audio or fragmented MP4.
The main benefit to fragmented MP4 is having a wider range of codecs
available, and the ability to encode loudness metadata. Both formats
allow adding timed ID3 metadata.

## Features

* Reads from standard input, files, and URLs via libCURL.
* Parses Icecast/Shoutcast metadata.
* Writes to standard output, files, folders, and URLs,
including [S3-compatible storage](https://github.com/jprjr/icecast-hls/wiki/Plugins:-Output#aws-default-false)
* Special support for chained [Ogg FLAC](https://github.com/jprjr/icecast-hls/wiki/Misc:-Ogg-Chaining-With-FLAC).
    * this allows rich in-band metadata and images.
* Apply filters with [libavfilter](https://ffmpeg.org/ffmpeg-filters.html)
* Multi-threaded encoding.
* Customizable [tag mapping](https://github.com/jprjr/icecast-hls/wiki/Configuration-Reference#tagmaps).
* Encoder plugins for:
    * [exhale](https://gitlab.com/ecodis/exhale) (USAC Audio)
    * [fdk-aac](https://github.com/mstorsjo/fdk-aac) (AAC, HE-AAC/V2)
    * [libavcodec](https://ffmpeg.org/ffmpeg-codecs.html) (AAC, MP3, (E)-AC-3, FLAC, ALAC, Opus)
* Output to packed audio and/or fragmented MP4.
* Embed timed ID3 metadata.
* Embed album art (either in-band or as a linked image).

## Documentation

I've tried to write as extensive documentation as possible as
a [wiki](https://github.com/jprjr/icecast-hls/wiki/Documentation).
It includes details on every plugin and its options, as well as tips
on how to produce chained Ogg streams with FLAC audio.

## License

MIT, see the file `LICENSE`.

Some files are third-party and have their own licensing:

* `src/miniflac.h`: BSD Zero Clause License.
* `src/minifmp4.h`: BSD Zero Clause License.
* `src/ini.h`: New BSD License.
* `src/thread.h`: Public Domain
