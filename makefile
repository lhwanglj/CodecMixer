#**********************************************************
#
# @brief   	makefile for common library
# @author  	wanglijun@feinno.com
# @version	v1.0
# @data		2010-06-30
#**********************************************************


#ctags -R -f ~/.vim/tags/mycommontags --c++-kinds=+p --fields=+iaS --extra=+q --language-force=C++ /home/wlj/code/home/common/


# define DEBUG flag, DEBUG flag can be set YES to include debugging info, or NO otherwise
DEBUG = YES

# define some command variable
CC		= gcc
CXX		= g++
AR 		= ar rc
MKDIR  	= mkdir -p
CP 		= cp -rf
MV		= mv -rf
RM		= rm -rf

# define some parameter variable 
DEBUG_CFLAGS	= -g -O0 -std=c++11 -D POSIX 
#DEBUG_CFLAGS	= -Wall -Wno-format -g -DDEBUG -Wreorder
RELEASE_CFLAGS	= -Wall -Wno-unknown-pragmas -Wno-format -O3

ifeq (YES, $(DEBUG))
	CFLAGS	= $(DEBUG_CFLAGS)
else
	CFLAGS	= $(RELEASE_CFLAGS)
endif

# define INCLUDES_FLAGS flags, INCLUDES_FLAGS flags include head files's directory. it is the path for search head files. it is include both source file's head files and library's head files.
INCLUDES_FLAGS	= 	-I./ \
					-I./include \
					-I./AVSDK/avutil/include \
					-I./AVSDK/avutil/include/src/fecrq \
					-I./AVSDK/avutil/include/fecrq \
					-I./AVSDK \
					-I./AVSDK/avmixer \
                    -I./AVSDK/MediaIO
 
                     

# define LIBSPATH_FLAGS flags, LIBSPATH_FLAGS flags include library's directory. it is the path for search library file.
# define LIBS_FLAGS flags, LIBS_FLAGS flags include all library files.
LIBSPATH_FLAGS	= -L /home/wlj1/codec/MediaCodec/build/linux/lib  

LIBS_FLAGS		= -lpthread -lrt -lmediacommon -lprotobuf -ldl

# define source file variable
 SRCFILES        =  ./AVSDK/avutil/src/avcommon.cpp \
					./AVSDK/avutil/src/BaseSocket.cpp \
					./AVSDK/avutil/src/buffer.cpp \
					./AVSDK/avutil/src/clock.cpp \
					./AVSDK/avutil/src/common.cpp \
					./AVSDK/avutil/src/datetime.cpp \
					./AVSDK/avutil/src/file.cpp \
					./AVSDK/avutil/src/fmem.cpp \
					./AVSDK/avutil/src/http.cpp \
					./AVSDK/avutil/src/i420reader.cpp \
					./AVSDK/avutil/src/i420writer.cpp \
					./AVSDK/avutil/src/LogFile.cpp \
					./AVSDK/avutil/src/logging.cpp \
					./AVSDK/avutil/src/mqthread_posix.cpp \
					./AVSDK/avutil/src/mqthread_win32.cpp \
					./AVSDK/avutil/src/msgqueue.cpp \
					./AVSDK/avutil/src/mstring.cpp \
					./AVSDK/avutil/src/mstringlist.cpp \
					./AVSDK/avutil/src/protocol.cpp \
					./AVSDK/avutil/src/rc4.cpp \
					./AVSDK/avutil/src/rsa.cpp \
					./AVSDK/avutil/src/socket.cpp \
					./AVSDK/avutil/src/socket_win32.cpp \
					./AVSDK/avutil/src/sps.cpp \
					./AVSDK/avutil/src/sync.cpp \
					./AVSDK/avutil/src/thread.cpp \
					./AVSDK/avutil/src/waveoperator.cpp \
					./AVSDK/avutil/src/wavreader.c \
					./AVSDK/avutil/src/fecrq/fecrepo_8_200.cpp \
					./AVSDK/avutil/src/fecrq/fecrepo_reader.cpp \
					./AVSDK/avutil/src/fecrq/fecrq.cpp \
					./AVSDK/avutil/src/fecrq/fecrqa.cpp \
					./AVSDK/avutil/src/fecrq/fecrqc.cpp \
					./AVSDK/avutil/src/fecrq/fecrqm.cpp \
					./AVSDK/avutil/src/fecrq/fecrqt.cpp \
                    ./AVSDK/MediaIO/File/Mp3FileIO.cpp\
                    ./AVSDK/MediaIO/net/NetTransfer.cpp\
                    ./AVSDK/MediaIO/net/SocketHandle.cpp\
                    ./AVSDK/MediaIO/net/srs_librtmp.cpp\
                    ./AVSDK/MediaIO/hpsp/streamframe.cpp\
                    ./AVSDK/MediaIO/IMediaIO.cpp\
                    ./AVSDK/avmixer/audiomixer.cpp\
                    ./AVSDK/avmixer/generalaudiomixer.cpp\
                    ./AVSDK/avmixer/picturemixer.cpp\
                    ./AVSDK/src/Adapter/audiopacketbuffer.cpp\
                    ./src/avmnetpeer.cpp\
                    ./src/avmmixer.cpp\
                    ./src/avmprocess.cpp\
                    ./src/Ini.cpp\
                    ./src/commonstruct.cpp\
                    ./src/MutexObj.cpp\
                    ./src/Log.cpp\
                    ./src/Channel.cpp\
                    ./src/TcpChannel.cpp\
                    ./src/BufSerialize.cpp\
                    ./src/avmBizsChannel.cpp\
                    ./src/avmGridChannel.cpp\
                    ./src/AVMixer2BizsMessage.cpp\
                    ./src/rgridproto.cpp\
					./src/main.cpp
                   

OBJS 			= $(addsuffix .o, $(basename $(SRCFILES)))
LIBOBJS 		= $(addsuffix .o, $(basename $(SRCFILES)))
LIBHEADFILES 	= $(addsuffix .h, $(basename $(SRCFILES))) 

# define targets of the build
TARGET			= pushrtmp 
TARGET_DIR		= ./bin
#TARGET_INCLUDES = ./include
#LIB				= libmycommon.a


# command the rules for link objs file to target or lib
all: $(TARGET) $(LIB)

$(TARGET) : $(OBJS)
	if [ ! -d $(TARGET_DIR) ]; then $(MKDIR) $(TARGET_DIR); fi
	$(CXX) -o $@ $(CFLAGS) $(INCLUDES_FLAGS) $(OBJS) $(LIBSPATH_FLAGS) $(LIBS_FLAGS)
	$(CP) $@ $(TARGET_DIR)

$(LIB) : $(LIBOBJS)
	if [ ! -d $(TARGET_DIR) ]; then $(MKDIR) $(TARGET_DIR); fi
	if [ ! -d $(TARGET_INCLUDES) ]; then $(MKDIR) $(TARGET_INCLUDES); fi
	$(AR) $@ $(LIBOBJS)
	$(CP) $@ $(TARGET_DIR)
	$(CP) $(LIBHEADFILES) $(TARGET_INCLUDES)
	$(CP) ./template/*.cpp $(TARGET_INCLUDES)

# rules for compiling source files to object files
%.o : %.cpp
	$(CXX) -c $(CFLAGS) $(INCLUDES_FLAGS) $< -o $@
#	$(CXX) -c $(CFLAGS) $< -o $@

%.o : %.c
	$(CC) -c $(CFLAGS) $(INCLUDES_FLAGS) $< -o $@

.PHONY: clean

clean:
	$(RM) $(TARGET) $(LIB) $(LIBOBJS) 
	$(RM) $(TARGET_DIR)/$(TARGET) $(TARGET_INCLUDES)



