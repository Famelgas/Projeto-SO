FLAGS   = -Wall -g
CC      = gcc
PROG    = offload_simulator
OBJS    = offloading_simulator.o functions.o manager.o mobile_node.o 

all:    ${PROG}

clean:
	rm ${OBJS} ${PROG} *~

${PROG}:        ${OBJS}
	${CC} ${FLAGS} ${OBJS} -o $@

.c.o:
	${CC} ${FLAGS} $< -c

###################################################################

offloading_simulator.o: declarations.h offloading_simulator.c

functions.o: declarations.h functions.c

manager.o: declarations.h manager.c

mobile_node.o: declarations.h mobile_node.c

offload_simulator: edge_server.o offloading_simulator.o manager.o mobile_node.o