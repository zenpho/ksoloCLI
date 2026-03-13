# ksoloCLI
Commandline communication with ksoloti and axoloti (without java editor)

ksoloCLI is written in c and depends on libusb for communication

The software is very stable and extensively tested with ksoloti hardware on MacOS 10.7 10.9 10.13 and 10.14. The binary supplied in the github releases is compatible with x86_64 intel MacOS 10.13.

```
ksolocli - zenpho@zenpho.co.uk - 2026 mar 13

============  actions  ============
'd' debug    'b' binfile  'p' ping 
'v' version  'S' STOP     's' start
'!' re-init  'q' quit     'h' help 

=========== reminder ===========

 files named start.bin replace
 and autostart from SDTF CARD

======== bin file choice ========
1. drag-and-drop file
2. confirm path correct
3. press enter when ready
Filepath: ~/path/to/some/start.bin

OK - start.bin ready. Stopped patch.
========= restart manually =========
====  to confirm start.bin load  ===
========= restart manually =========
```

# Usage

Simply start `./kzolocli` then plug in your axo/kso hardware and wait for a USB connection to be established before pressing the 'b' key to send any `xpatch.bin` that you have saved and kept. You might keep these with DAW project files and MIDI sequences.

Uploading a file named `start.bin` will replace sd-card standalone autostart files and requires manual hw restart.

# Acknowledgements

for more information about axoloti and ksoloti platforms
- see [github.com/axoloti/axoloti](//github.com/axoloti/axoloti)
- and [github.com/ksoloti/ksoloti](//github.com/ksoloti/ksoloti)

see also [my modified v1.0.12 ksoloti firmware](//github.com/zenpho/ks1.0.12) and [my AxoPanelControls system](//github.com/zenpho/AxoPanelControls)
