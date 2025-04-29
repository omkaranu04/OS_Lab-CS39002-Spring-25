all: parent.c child.c dummy.c
	gcc parent.c -o parent
	gcc child.c -o child
	gcc dummy.c -o dummy

run: all
	./parent 10

clean:
	rm -f parent child dummy childpid.txt dummycpid.txt