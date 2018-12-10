CC=g++
FILE=main.cc LHE_codec/src/lhe_codec.cc LHE_codec_core/src/lhe_codec_core.cc
INCLUDE=-I /opt/opencv/include
LIB_PATH=-L /opt/opencv/lib
LIB=-lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_highgui
FLAGS=-Werror `pkg-config --cflags --libs opencv`
DEBUG_FLAGS= -DDEBUG
OPTIMIZATION=-O3
OBJ=mock_gen.out 

all:
	$(CC) $(FILE) $(INCLUDE) $(FLAGS) $(LIB_PATH) $(LIB) $(DEBUG_FLAGS) $(OPTIMIZATION) -o $(OBJ)

clean:
	rm -rf $(OBJ) *~*
