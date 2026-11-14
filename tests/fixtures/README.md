# Video track fixture

`video-tracks.mkv` is test-only media generated locally from solid colors,
silence and the caption text below. It contains 20 seconds of 160×96 FFV1 video
(red top, green bottom), two silent FLAC audio tracks tagged `eng`/`jpn`, and
English/French SubRip subtitle tracks. It contains no third-party media assets.
Tests use it directly; users running CTest do not need the FFmpeg executable.

Regenerate from the repository root with FFmpeg (originally 9.0.2):

```sh
fixture_dir=$(mktemp -d)
printf '1\n00:00:00,000 --> 00:00:19,000\nEnglish captions\n' > "$fixture_dir/english.srt"
printf '1\n00:00:00,000 --> 00:00:19,000\nSous-titres français\n' > "$fixture_dir/french.srt"
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=c=red:s=160x96:r=5:d=20,drawbox=x=0:y=48:w=160:h=48:color=green:t=fill' \
  -f lavfi -i 'anullsrc=r=8000:cl=mono' \
  -i "$fixture_dir/english.srt" -i "$fixture_dir/french.srt" \
  -map 0:v -map 1:a -map 1:a -map 2:s -map 3:s \
  -c:v ffv1 -c:a flac -c:s srt \
  -metadata:s:a:0 language=eng -metadata:s:a:0 title='English audio' \
  -metadata:s:a:1 language=jpn -metadata:s:a:1 title='Japanese audio' \
  -metadata:s:s:0 language=eng -metadata:s:s:0 title='English captions' \
  -metadata:s:s:1 language=fra -metadata:s:s:1 title='Français' \
  -t 20 -y tests/fixtures/video-tracks.mkv
```

The native smoke test can additionally load either generated SRT file:

```sh
./build-video/tests/cui_mpv_smoke tests/fixtures/video-tracks.mkv /tmp/subtitles.png "$fixture_dir/english.srt"
```
