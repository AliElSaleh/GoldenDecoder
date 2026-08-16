# Golden Decoder

An interactive application that decodes the audio images that were encoded onto the Golden Record by NASA. This program reads the audio samples each scan-line at a time, to build up a final image, in real time.

Watch full decoding video: https://www.youtube.com/watch?v=coN2vCFyXmw

### Motivation
I saw many decoder projects out there that didn't meet my high expectations of an interactive low-level cross platform executable that you could run on all the major operating systems natively.
some were web only, others written in non-C languages, some were confusing to understand and/or using external libraries that were doing the audio parsing job for them, and some had jittery images.

Also, this was a fun side project for my brain to work on as I haven't done any programming relating to digital signal processing.

<img width="720" height="405" alt="golden-decoder-phys-tree-toad" src="https://github.com/user-attachments/assets/0b451864-ba38-48b5-ab3f-364ca7ef8984" />

<img width="720" height="405" alt="golden-decoder-menu-operations" src="https://github.com/user-attachments/assets/103881ef-93f2-43bb-93b2-96117f2c6bbf" />

# Building

This project uses my own build system that I created (because CMake/Make/Ninja etc. are hot garbage), you can grab the latest release from here https://github.com/AliElSaleh/RiftBuild

Once you have the binary in your path, just run:
```
riftbuild
```

#### Supported Architectures
- x64
- arm64

#### Supported Platforms
- Windows 10 and above
- Linux (using X11 display server)
- macOS 11 (Big Sur) and above
