### Building:

```sh
mkdir build ; cd build
# CMake options:
# option(WATOR_BUILD_MAP_READER "Option to enable building of map_reader binary" ON) # enable support for png generation from map files, builds parwatorMapReader, requires libpng
# option(WATOR_CPU_PIN "Optimisation: Pin tasks to CPUS" ON)
# option(WATOR_NUMA "Add support for NUMA" ON)   # enable NUMA support, requires libnuma
# option(WATOR_NUMA_OPTIMIZE "Optimize when the NUMA node is only one" ON) # disabled only for testing, leave on
# add CFLAGS or CXXFLAGS
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```
### Running:

```sh
# running the simulation:
# usually when height is the larger dimention, the simulation runs a little bit faster

app/parwator --height 1920 --width 1080 --itercnt 50 --output /tmp/gamemap.map
# or
app/parwator --height 208 --width 117 --itercnt 1000 --output /tmp/gamemap.map

# generating folder with png images for every frame
app/parwatorMapReader /tmp/gamemap.map /tmp/mapi

# combine png images into a video with ffmpeg:
# we need to rotate the video, also
ffmpeg -r 15 -f image2 -s 1920x1080 -i /tmp/mapi/%d.png -vcodec libx264 -crf 16 -pix_fmt rgb24 -vf transpose=1 vid.mp4
# or
# here we upscale the image using neighbor upscaling algorithm
ffmpeg -r 15 -f image2 -i /tmp/mapi/%d.png -vcodec libx264 -crf 16 -pix_fmt rgb24 -vf transpose=1,scale=1280:720 -sws_flags neighbor vid.mp4

# Now the video is ready, open vid.mp4
```

### Usage:
```sh
app/parwator --help
Usage: parwator [-h] --height VAR --width VAR --itercnt VAR [--fish VAR] [--sharks VAR] [--fishbreed VAR] [--sharkbreed VAR] [--sharkstarve VAR] [--threads VAR] [--enable-ht] [--seed VAR] [--output VAR] [--benchmark]

Optional arguments:
  -h, --help            shows help message and exits 
  -v, --version         prints version information and exits 
  --height              Height of the ocean [required]
  --width               Width of the ocean [required]
  --itercnt             Number of chronons to simulate [required]
  --fish                The initial number of fish in the ocean, default value is 1/10-th of ocean cells 
  --sharks              The initial number of sharks in the ocean, default value is 1/30-th of ocean cells 
  --fishbreed           The number of chronons have to pass for fish to be able to breed [default: 3]
  --sharkbreed          The number of chronons have to pass for shark to be able to breed [default: 10]
  --sharkstarve         The number of chronons have to pass for a shark must not eat to die [default: 3]
  --threads, --workers  Number of threads to run the simulation on, by default it uses all [default: 12]
  -H, --enable-ht       Enables the use of hyperthreaded cores 
  --seed                Provides seed for random number generation, warning: output is depending also on thread count 
  --output              Where to output the saved map [default: "/dev/null"]
  --benchmark           Gives significantly shorted output
```
