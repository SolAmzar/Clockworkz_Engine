For the audio implementation to work, place the fmod folder you see here into build.

Just COPY and PASTE it into the folder. (DO NOT remove/cut the file)

After that, run the run_build.bat. I wrote stuff into the cmakelist to include Fmod's libraries into the dependencies and setting for visual studio
(Specifically for c/c++ general, linker and post-build event)


<repo>/
  build/
	fmod/    <---- and PASTE it in the build file
	x64/
	assets/
	bin/
	
  src/
  include/
  fmod/ (COPY this file)

You only need to do this the first time. After that, the fmod folder should always be in build for you