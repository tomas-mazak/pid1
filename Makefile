INSDIR=/usr/local/bin
ifdef DESTDIR
	INSDIR=/bin
endif
ifdef DEBUG
	DEBUGME=-D DEBUG
endif

pid1: pid1.c
	gcc -Wall -Wextra -Werror $(DEBUGME) -o $@ pid1.c

install: pid1
	install -d -o root -m 0755 $(DESTDIR)$(INSDIR)
	install -o root -m 0755 pid1 $(DESTDIR)$(INSDIR)

clean:
	rm -f pid1
