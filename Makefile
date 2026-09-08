all: user

user: user.c
	gcc user.c -o user

clean:
	rm -f user