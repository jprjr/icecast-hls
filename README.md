# icecast-hls

Note!! This is still a work in progress, I just figured I'm far enough
along that I need to save my work.

This is a program that accepts an Icecast stream and re-encodes it
into 1 or more HLS streams.

It's main use is with chained Ogg files with FLAC audio, which
allows for in-band metadata, images, etc.

The HLS streams use fragmented mp4, which allows for a wide range
of codecs, along with timed ID3 metadata. This program will
read Vorbis comments and picture blocks from FLAC and convert
them into timed ID3 tags.

In addition to ID3 metadata, this can also (optionally) encode
loudness metadata, if you have it. So if you have a source
Icecast stream that you know is normalized with ReplayGain,
you can communicate that, and allow devices to adjust
accordingly.

## Producing Chained, FLAC-in-Ogg Bitstreams

A neat feature of Ogg streams is so-called "chaining." You can concatenate files and
players should play them back as a single, continuous stream. This is commonly used
with Opus and Vorbis Icecast streams, to provide in-band metadata, rather than the
legacy Shoutcast/Icecast metadata, which is essentially an unstructured string.

According to the specs, chaining should be supported by FLAC, but support is very
hit-or-miss so it's not widely implemented. So I've documented a few ways to create
compatible streams for `icecast-hls`.

In all these examples I'm assuming your source files are the same
sample rate, bit depth, and channel count.

### Using the FLAC CLI

The `flac` encoder can create FLAC-in-Ogg files, using the `--ogg` switch. You'll
want to specify any tags here, since the `metaflac` tool is unable to update
metadata in Ogg files:

```
flac --picture=cover.png --tag="TITLE=Title 1" --tag="ARTIST=Artist 1" --ogg -o track-01.ogg track-01.wav
flac --picture=cover.png --tag="TITLE=Title 2" --tag="ARTIST=Artist 2" --ogg -o track-02.ogg track-02.wav
```

You can then concatenate these files together:

```
cat *.ogg > my-chained-audio.ogg
```

### Using ffmpeg

`ffmpeg` can produce FLAC-in-Ogg files as well. This winds up being very useful, since
it will parse any existing tags in your source files, allow you to apply filters, and
so on. One useful feature is to re-mux existing FLAC files into Ogg files.

The one downside is, I haven't figured out how to get ffmpeg to keep cover art -
it seems to re-encode it into Ogg Theora, rather than just embed it.
You'll likely need to specify the `-vn` switch to disable any video processing.

```
ffmpeg -i track-01.wav -c:a flac -vn -f ogg track-01.ogg
# example of just re-muxing an existing flac file:
ffmpeg -i track-02.flac -c:a copy -vn -f ogg track-02.ogg
```

And similar to above, concatenate these together:

```
cat *.ogg > my-chained-audio.ogg
```

### Using Music Player Daemon

[Music Player Daemon](https://www.musicpd.org/) (as of version 0.23) can produce a chained Ogg
stream with FLAC audio.

As an example, you could configure an httpd output like:

```
audio_output {
        type            "httpd"
        name            "FLAC HTTP Stream"
        encoder         "flac"
        port            "8000"
        bind_to_address "127.0.0.1"
        format          "48000:16:2"
        max_clients     "0"
        compression     "0"
        oggchaining     "yes"
}
```

The important part is that "oggchaining" is set to "yes". You can
also do this with the "shout" type and a full-fledged Icecast
server.

## License

MIT, see the file `LICENSE`.

Some files are third-party and have their own licensing:

* `src/miniflac.h`: BSD Zero Clause License.
* `src/minifmp4.h`: BSD Zero Clause License.
* `src/ini.h`: New BSD License.
* `src/thread.h`: Public Domain
