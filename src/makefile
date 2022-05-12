FLAGS   = -pthread -Wall -g
CC      = gcc
PROG    = offload_simulator
OBJS    = offloading_simulator.o mobile_node.o 

all:    ${PROG}

clean:
	rm ${OBJS} ${PROG} *~

${PROG}:        ${OBJS}
	${CC} ${FLAGS} ${OBJS} -o $@

.c.o:
	${CC} ${FLAGS} $< -c

###################################################################

mobile_node.o: mobile_node.c

offloading_simulator.o: offloading_simulator.c declarations.h

mobile_node: mobile_node.o

offload_simulator: offloading_simulator.o