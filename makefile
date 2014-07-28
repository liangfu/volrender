LIBS = -lvtkRendering -lvtkImaging -lvtkIO -lvtkFiltering -lvtkCommon -lvtkVolumeRendering 

volrender: volrender.cpp
	g++ -Wall -g -o $@ $< -I/usr/include/vtk-5.8 $(LIBS)

.PHONY: volrender
