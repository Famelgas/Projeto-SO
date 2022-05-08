FLAGS   = -Wall -g
CC      = gcc
PROG    = offload_simulator
OBJS    = edge_server.o offloading_simulator.o manager.o mobile_node.o 

all:    ${PROG}

clean:
        rm ${OBJS} ${PROG} *~

${PROG}:        ${OBJS}
        ${CC} ${FLAGS} ${OBJS} -o $@

.c.o:
        ${CC} ${FLAGS} $< -c

###################################################################

edge_server.o: declarations.h edge_server.c

offloading_simulator.o: declarations.h offloading_simulator.c

manager.o: declarations.h managar.c

mobile_node.o: declarations.h mobile_node.c

offload_simulator: edge_server.o offloading_simulator.o manager.o mobile_node.o