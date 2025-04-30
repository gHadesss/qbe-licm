QBESRC = qbe
BUILDDIR = $(QBESRC)/build
BIN = qbe
LIB = libqbe.a
INC := qbe

V = @
OBJDIR = $(QBESRC)/obj

SRC      = main.c util.c parse.c cfg.c mem.c ssa.c alias.c load.c copy.c \
           fold.c live.c spill.c rega.c gas.c
AMD64SRC = amd64/targ.c amd64/sysv.c amd64/isel.c amd64/emit.c
ARM64SRC = arm64/targ.c arm64/abi.c arm64/isel.c arm64/emit.c
SRCALL   = $(SRC:%.c=$(QBESRC)/%.c) $(AMD64SRC:%.c=$(QBESRC)/%.c) $(ARM64SRC:%.c=$(QBESRC)/%.c)

AMD64OBJ = $(AMD64SRC:%.c=$(OBJDIR)/%.o)
ARM64OBJ = $(ARM64SRC:%.c=$(OBJDIR)/%.o)
OBJ      = $(SRC:%.c=$(OBJDIR)/%.o) $(AMD64OBJ) $(ARM64OBJ)

ARFLAGS = $(if $(V),crs,crsv)
CFLAGS += -Wall -Wextra -std=c99 -g -pedantic -DDEBUG

all: build

$(OBJDIR)/$(BIN): $(OBJ) $(OBJDIR)/timestamp
	@test -z "$(V)" || echo "ld $@"
	$(V)$(CC) $(LDFLAGS) $(OBJ) -o $@

$(OBJDIR)/$(LIB): $(OBJ) $(OBJDIR)/timestamp
	@test -z "$(V)" || echo "ar $@"
	$(V)$(AR) $(ARFLAGS) $@ $(filter %.o,$^)
	@test -z "$(V)" || echo "strip $@"
	$(V)strip --strip-symbol=main $@

$(OBJDIR)/%.o: $(QBESRC)/%.c $(OBJDIR)/timestamp
	@test -z "$(V)" || echo "cc $<"
	$(V)$(CC) --verbose $(CFLAGS) -c $< -o $@

$(OBJDIR)/timestamp:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/amd64
	@mkdir -p $(OBJDIR)/arm64
	@touch $@

$(OBJ): $(QBESRC)/all.h $(QBESRC)/ops.h
$(AMD64OBJ): $(QBESRC)/amd64/all.h
$(ARM64OBJ): $(QBESRC)/arm64/all.h
$(QBESRC)/obj/main.o: $(QBESRC)/config.h

$(QBESRC)/config.h:
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
	mkdir -p "$(BUILDDIR)/bin/" "$(BUILDDIR)/lib/" "$(BUILDDIR)/include/$(INC)/"
	cp "$(OBJDIR)/$(BIN)" "$(BUILDDIR)/bin/"
	cp "$(OBJDIR)/$(LIB)" "$(BUILDDIR)/lib/"
	cp $(QBESRC)/all.h $(QBESRC)/ops.h "$(BUILDDIR)/include/$(INC)"

uninstall:
	rm -fr "$(BUILDDIR)/bin/$(BIN)" "$(BUILDDIR)/lib/$(LIB)" "$(BUILDDIR)/include/$(INC)"

build: install
	@test -z "$(V)" || echo "cxx main.cpp"
	$(V)$(CXX) $(CFLAGS) -I$(BUILDDIR)/include -L$(BUILDDIR)/lib main.cpp -lqbe -o main

clean:
	rm -fr $(OBJDIR)
	rm -rf $(BUILDDIR)
	rm -f main

clean-gen: clean
	rm -f $(QBESRC)/config.h

check: $(OBJDIR)/$(BIN)
	$(QBESRC)/tools/test.sh all

check-arm64: $(OBJDIR)/$(BIN)
	TARGET=arm64 $(QBESRC)/tools/test.sh all

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

.PHONY: all clean clean-gen check check-arm64 src 80 build install uninstall
