CUR_DIR=/mnt/hgfs/share/WebServer

COMMON_DIR=${CUR_DIR}/common
LOG_DIR=${CUR_DIR}/log
DB_DIR=${CUR_DIR}/database
CLIENT_DIR=${CUR_DIR}/client
SERVER_DIR=${CUR_DIR}/server
CONFIG_DIR=${CUR_DIR}/config

INC_DIR= -I ${CUR_DIR} #-I${COMMON_DIR} -I${LOG_DIR} -I${DB_DIR}

SRC=${wildcard ${COMMON_DIR}/*.cpp ${LOG_DIR}/*.cpp ${DB_DIR}/*.cpp ${CLIENT_DIR}/*.cpp ${SERVER_DIR}/*.cpp}

OBJ=${patsubst %.cpp,%.o,${SRC}}

TARGET=WebServer
CC=g++
CCFLAGES=-std=c++11  -g -lpthread -lmysqlclient ${INC_DIR} -O2

${TARGET}:${OBJ}
	${CC} main.cpp ${notdir ${OBJ}} $(CCFLAGES) -o $@

%.o:%.cpp
	$(CC) $(CCFLAGES) -c $?

.PHONY:clean
clean:
	rm server -f