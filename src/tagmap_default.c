#include "tagmap_default.h"

taglist _DEFAULT_TAGMAP;
taglist* DEFAULT_TAGMAP = &_DEFAULT_TAGMAP;

int default_tagmap_init(void) {
    int r;
    taglist_init(DEFAULT_TAGMAP);
#define ADDP(k,v,p) if( (r = taglist_add_cstr_priority(DEFAULT_TAGMAP,#k,#v,p)) != 0) return r;
#define ADD(k,v) ADDP(k,v,0xFF)

    ADD(album,TALB)
    ADD(albumsort,TSOA)
    ADD(discsubtitle,TSST)
    ADD(grouping,GRP1)
    ADD(work,TIT1)
    ADD(title,TIT2)
    ADD(titlesort,TSOT)
    ADD(subtitle,TIT3)
    ADD(movementname,MVNM)
    ADD(albumartist,TPE2)
    ADD(albumartistsort,TSO2)
    ADD(artist,TPE1)
    ADD(artistsort,TSOP)
    ADD(composer,TCOM)
    ADD(conductor,TPE3)
    ADD(label,TPUB)
    ADD(remixer,TPE4)
    ADD(discnumber,TPOS)
    ADD(tracknumber,TRCK)
    ADD(movement,MVIN)
    ADDP(date,TDRC,0xFE)
    ADD(year,TDRC)
    ADD(originaldate,TDOR)
    ADD(isrc,TSRC)
    ADD(compilation,TCMP)
    ADD(encoded-by,TENC)
    ADD(encoder,TSSE)
    ADD(media,TMED)
    ADD(comment,TXXX:comment)
    ADD(replaygain_album_gain,TXXX:replaygain_album_gain)
    ADD(replaygain_album_peak,TXXX:replaygain_album_peak)
    ADD(replaygain_album_range,TXXX:replaygain_album_range)
    ADD(replaygain_track_gain,TXXX:replaygain_track_gain)
    ADD(replaygain_track_peak,TXXX:replaygain_track_peak)
    ADD(replaygain_track_range,TXXX:replaygain_track_range)
    ADD(replaygain_reference_loudness,TXXX:replaygain_reference_loudness)
    ADD(genre,TCON)
    ADD(bpm,TBPM)
    ADD(mood,TMOO)
    ADD(copyright,TCOP)
    ADD(language,TLAN)
    ADD(lyrics,USLT)
    ADD(metadata_picture_block,APIC)
#undef ADD
    taglist_sort(DEFAULT_TAGMAP);
    return 0;
}

void default_tagmap_deinit(void) {
    taglist_free(DEFAULT_TAGMAP);
}
