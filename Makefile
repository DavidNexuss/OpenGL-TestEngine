main: main.cc
	g++ main.cc -O3 -msse4 -mavx2 -fopenmp -lGL -lglfw -lGLU -lGLEW -o main
clean:
	rm main
