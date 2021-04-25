main: main.cc
	g++ main.cc -O3 -msse4 -mavx2 -fopenmp -lGL -lglfw -lGLU -lGLEW imgui.a -o main
debug:
	g++ main.cc -g -lGL -lglfw -lGLU -lGLEW imgui.a -o main
dis:	
	g++ main.cc -g -O3 -msse4 -mavx2 -fopenmp -lGL -lglfw -lGLU -lGLEW imgui.a -S -o main.S
clean:
	rm main
