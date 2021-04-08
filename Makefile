main: main.cc
	g++ main.cc -lGL -lglfw -lGLU -lGLEW -O2 -o main
clean:
	rm main
