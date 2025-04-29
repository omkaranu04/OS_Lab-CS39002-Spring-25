all: cook waiter customer gencustomers

cook: cook.c
	gcc -Wall -o cook cook.c

waiter: waiter.c
	gcc -Wall -o waiter waiter.c

customer: customer.c
	gcc -Wall -o customer customer.c

gencustomers: gencustomers.c
	gcc -Wall -o gencustomers gencustomers.c

db: gencustomers
	./gencustomers > customers.txt

clean:
	-rm -f cook waiter customer gencustomers customers.txt
