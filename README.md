# Golden Decoder

An interactive application that decodes the audio images that were encoded onto the Golden Record by NASA. This program reads the audio samples each scan-line at a time, to build up a final image, in real time.

### Motivation
I saw many decoders projects out there that didn't meet my high expectations of an interactive low-level cross platform executable that you could run on all the major operating systems natively.
some were web only, others written in non-C languages, some were confusing to understand and some had jittery images.

Also, this was a fun side project for my brain to work on as I haven't done any programming relating to digital signal processing.


<img width="720" height="405" alt="golden-decoder-phys-tree-toad" src="https://github.com/user-attachments/assets/0b451864-ba38-48b5-ab3f-364ca7ef8984" />

<img width="720" height="405" alt="golden-decoder-menu-operations" src="https://github.com/user-attachments/assets/103881ef-93f2-43bb-93b2-96117f2c6bbf" />



# Building

This project uses my own build system that I created, you can grab the latest release from here https://github.com/AliElSaleh/RiftBuild

Once you have the binary in your path, just run:
```
riftbuild
```

otherwise, you can just download the latest binary release
