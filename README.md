```sh
parwatorCG 3 false 200 400 4760 470 3 10 3 800
parwatorMapReader /tmp/gamemap.map /tmp/mapi
ffmpeg -r 15 -f image2 -s 1920x1080 -i /tmp/mapi/%d.png -vcodec libx264 -crf 16 -pix_fmt rgb24 -vf transpose=1 vid.mp4
```
