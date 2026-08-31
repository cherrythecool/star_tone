# star tone

music converter & probably other stuff too eventually, mostly just making for my own personal use
(i could just write a shell script that does weird ffmpeg stuff but idk this is kinda fun too)

written in c99

currently only testing on macOS, probably works only on other POSIX systems
since i'm using POSIX specific stuff in the code atm (might change in future idk)

contains modified versions of thirdparty libraries but the changes are minimal besides in `cherryaudio`
so it's whatever

## building

just build with cmake, `cmake -B build` & `cmake --build build` should be all you need i think