BIN = qbe
LIB = libqbe.a
INC := qbe

V = @
OBJDIR = obj

SRC      = main.c util.c parse.c cfg.c mem.c ssa.c alias.c load.c copy.c \
           fold.c live.c spill.c rega.c gas.c
AMD64SRC = amd64/targ.c amd64/sysv.c amd64/isel.c amd64/emit.c
ARM64SRC = arm64/targ.c arm64/abi.c arm64/isel.c arm64/emit.c
SRCALL   = $(SRC) $(AMD64SRC) $(ARM64SRC)

AMD64OBJ = $(AMD64SRC:%.c=$(OBJDIR)/%.o)
ARM64OBJ = $(ARM64SRC:%.c=$(OBJDIR)/%.o)
OBJ      = $(SRC:%.c=$(OBJDIR)/%.o) $(AMD64OBJ) $(ARM64OBJ)

ARFLAGS = $(if $(V),crs,crsv)
CFLAGS += -Wall -Wextra -std=c99 -g -pedantic

all: $(OBJDIR)/$(BIN) $(OBJDIR)/$(LIB)

$(OBJDIR)/$(BIN): $(OBJ) $(OBJDIR)/timestamp
	@test -z "$(V)" || echo "ld $@"
	$(V)$(CC) $(LDFLAGS) $(OBJ) -o $@

$(OBJDIR)/$(LIB): $(OBJ) $(OBJDIR)/timestamp
	@test -z "$(V)" || echo "ar $@"
	$(V)$(AR) $(ARFLAGS) $@ $(filter %.o,$^)
	@test -z "$(V)" || echo "strip $@"
	$(V)strip --strip-symbol=main $@

$(OBJDIR)/%.o: %.c $(OBJDIR)/timestamp
	@test -z "$(V)" || echo "cc $<"
	$(V)$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/timestamp:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/amd64
	@mkdir -p $(OBJDIR)/arm64
	@touch $@

$(OBJ): all.h ops.h
$(AMD64OBJ): amd64/all.h
$(ARM64OBJ): arm64/all.h
obj/main.o: config.h

config.h:
	@case `uname` in                               \
	*Darwin*)                                      \
		echo "#define Defasm Gasmacho";        \
		echo "#define Deftgt T_amd64_sysv";    \
		;;                                     \
	*)                                             \
		echo "#define Defasm Gaself";          \
		case `uname -m` in                     \
		*aarch64*)                             \
			echo "$define Deftgt T_arm64"; \
			;;                             \
		*)                                     \
			echo "#define Deftgt T_amd64_sysv";\
			;;                             \
		esac                                   \
		;;                                     \
	esac > $@

install: $(OBJDIR)/$(BIN) $(OBJDIR)/$(LIB)
	mkdir -p "$(DESTDIR)/$(PREFIX)/bin/" "$(DESTDIR)/$(PREFIX)/lib/" "$(DESTDIR)/$(PREFIX)/include/$(INC)/"
	cp "$(OBJDIR)/$(BIN)" "$(DESTDIR)/$(PREFIX)/bin/"
	cp "$(OBJDIR)/$(LIB)" "$(DESTDIR)/$(PREFIX)/lib/"
	cp all.h ops.h "$(DESTDIR)/$(PREFIX)/include/$(INC)"

uninstall:
	rm -fr "$(DESTDIR)/$(PREFIX)/bin/$(BIN)" "$(DESTDIR)/$(PREFIX)/lib/$(LIB)" "$(DESTDIR)/$(PREFIX)/include/$(INC)"

clean:
	rm -fr $(OBJDIR)

clean-gen: clean
	rm -f config.h

check: $(OBJDIR)/$(BIN)
	tools/test.sh all

check-arm64: $(OBJDIR)/$(BIN)
	TARGET=arm64 tools/test.sh all

src:
	@echo $(SRCALL)

80:
	@for F in $(SRCALL);                       \
	do                                         \
		awk "{                             \
			gsub(/\\t/, \"        \"); \
			if (length(\$$0) > $@)     \
				printf(\"$$F:%d: %s\\n\", NR, \$$0); \
		}" < $$F;                          \
	done

.PHONY: all clean clean-gen check check-arm64 src 80 install uninstall
