all : sender receiver

sender : sender.o stp.o packet.o buffer.o
	gcc -o sender sender.o stp.o packet.o buffer.o

receiver: receiver.o stp.o packet.o buffer.o
	gcc -o receiver receiver.o stp.o packet.o buffer.o

receiver.o : receiver.c stp.h packet.h buffer.h
	gcc -Wall -Werror -std=c11 -c receiver.c

sender.o : sender.c stp.h packet.h buffer.h
	gcc -Wall -Werror -std=c11 -c sender.c

stp.o : stp.c stp.h packet.h buffer.h
	gcc -Wall -Werror -std=c11 -c stp.c

buffer.o: buffer.c buffer.h packet.h
	gcc -Wall -Werror -std=c11 -c buffer.c

packet.o: packet.c packet.h
	gcc -Wall -Werror -std=c11 -c packet.c

clean:
	rm -f sender receiver sender.o stp.o packet.o receiver.o buffer.o Sender_log.txt Receiver_log.txt test0_r.pdf test1_r.pdf test2_r.pdf
