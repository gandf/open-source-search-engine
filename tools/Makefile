.DEFAULT_GOAL := all

TARGETS := $(sort $(patsubst %.cpp, %, $(wildcard *.cpp)))

BASE_DIR ?= ..

.PHONY: all
all: $(TARGETS)

.PHONY: libgb.a
libgb.a:
	$(MAKE) -C $(BASE_DIR) libgb.a

ucdata:
	ln -s $(BASE_DIR)/$@ .

tlds.txt:
	ln -sf $(BASE_DIR)/$@ .

$(BASE_DIR)/libcld2_full.so:
	$(MAKE) -C $(BASE_DIR) libcld2_full.so

$(BASE_DIR)/libcld3.so:
	$(MAKE) -C $(BASE_DIR) libcld3.so

$(BASE_DIR)/libced.so:
	$(MAKE) -C $(BASE_DIR) libced.so

CPPFLAGS += -g
CPPFLAGS += -Wall -Wno-write-strings
CPPFLAGS += -Wl,-rpath=. -Wl,-rpath=$(BASE_DIR)
CPPFLAGS += -I$(BASE_DIR) -I$(BASE_DIR)/third-party/compact_enc_det -I$(BASE_DIR)/word_variations -I$(BASE_DIR)/third-party/sparsepp -I$(BASE_DIR)/third-party/c-ares -I$(BASE_DIR)/tokenizer

CPPFLAGS += -std=c++11

# exported in parent make
CPPFLAGS += $(CONFIG_CPPFLAGS)

LIBS += $(BASE_DIR)/libgb.a -lz -lpthread -lssl -lcrypto -lpcre -ldl -lsqlite3 $(BASE_DIR)/libunicode.a
LIBS += -L$(BASE_DIR) -lcld2_full -lcld3 -lprotobuf -lced -lcares

%: libgb.a $(BASE_DIR)/libcld2_full.so $(BASE_DIR)/libcld3.so $(BASE_DIR)/libced.so %.cpp
	$(CXX) $(CPPFLAGS) $@.cpp $(LIBS) -o $@

.PHONY: clean
clean:
	rm -f *.o $(TARGETS) core.*
	rm -f *.gcda *.gcno

