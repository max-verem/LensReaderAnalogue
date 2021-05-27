# LensReaderAnalogue

This project is used to bind Lens with analogue output (like ) with Unreal Engine

## Schematics

![Main module](/schematic/module.png?raw=true "Main module")

![Cable for connecting with PC](/schematic/module_to_pc.png?raw=true "Cable for connecting with PC")

![Cable for connecting with LENS](/schematic/module_to_lens.png?raw=true "Cable for connecting with LENS")

## Device photo

![Device photo](/schematic/photo.jpeg?raw=true "Device photo")

## Running server

command line for running vrpn server (reader from serial)

```
VRPN-LensReaderAnalogue.exe 3887 \\.\COM5 233 4.3 836 60
```
output:
```
[...]
1599507646.0843 | Zoom: trg=20.8979 flt=412.6860 raw=413.0000 | Focus: trg=234.4357 flt=234.4357 raw=235.0000
1599507646.0859 | Zoom: trg=20.9034 flt=412.7456 raw=413.0000 | Focus: trg=234.4429 flt=234.4429 raw=234.0000
1599507646.0890 | Zoom: trg=20.8930 flt=412.6336 raw=412.0000 | Focus: trg=234.4129 flt=234.4129 raw=234.0000
1599507646.0906 | Zoom: trg=20.8994 flt=412.7032 raw=413.0000 | Focus: trg=234.4244 flt=234.4244 raw=234.0000
1599507646.0922 | Zoom: trg=20.8963 flt=412.6696 raw=413.0000 | Focus: trg=234.4338 flt=234.4338 raw=234.0000
[...]
```




