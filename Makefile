# C99Term — st fork with an Alacritty-compatible config
# See src/LICENSE for the original st MIT/X notice.
.POSIX:

include config.mk

SRC = src/st.c src/x.c src/alacritty.c
OBJ = $(SRC:.c=.o)

all: c99term

src/config.h:
	cp src/config.def.h src/config.h

.c.o:
	$(CC) $(STCFLAGS) -c $< -o $@

src/st.o: src/config.h src/st.h src/win.h
src/x.o: src/arg.h src/config.h src/st.h src/win.h src/alacritty.h
src/alacritty.o: src/alacritty.h

$(OBJ): src/config.h config.mk

c99term: $(OBJ)
	$(CC) -o $@ $(OBJ) $(STLDFLAGS)

test: tests/test_config.c src/alacritty.c src/alacritty.h
	$(CC) -std=c99 -Wall -Wextra -DALA_NO_X11 -D_XOPEN_SOURCE=600 -Isrc \
		-o /tmp/c99term-test tests/test_config.c src/alacritty.c
	/tmp/c99term-test

clean:
	rm -f c99term $(OBJ) tests/test_config src/config.h

install: c99term
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f c99term $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/c99term
	ln -sf c99term $(DESTDIR)$(PREFIX)/bin/st
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < src/st.1 > $(DESTDIR)$(MANPREFIX)/man1/c99term.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/c99term.1
	tic -sx src/st.info

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/c99term
	rm -f $(DESTDIR)$(PREFIX)/bin/st
	rm -f $(DESTDIR)$(MANPREFIX)/man1/c99term.1

.PHONY: all clean install uninstall test
