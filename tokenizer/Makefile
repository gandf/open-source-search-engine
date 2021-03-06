ifeq ($(config),$(filter $(config),release release-safe))
CPPFLAGS += -O3 -march=core-avx-i -msse4.2
else
CPPFLAGS += -O0
endif
CPPFLAGS += -g
CPPFLAGS += -std=c++11
CPPFLAGS += -Wall
CPPFLAGS += -I.. -I../unicode

.cpp.o:
	$(CXX) $(DEFS) $(CPPFLAGS) -c $*.cpp


.PHONY: default
default: libtokenizer.a

libtokenizer.a: tokenizer.o tokenizer2.o tokenizer3.o tokenizer4.o tokenizer5.o tokenizer_util.o
	ar rcs $@ tokenizer.o tokenizer2.o tokenizer3.o tokenizer4.o tokenizer5.o tokenizer_util.o


tokenizer_unittest: tokenizer.o tokenizer2.o tokenizer_unittest.o
	g++ -g tokenizer.o tokenizer2.o tokenizer_unittest.o ../unicode/libunicode.a ../utf8_fast.o ../utf8.o ../EGStack.o -o $@

xml_tokenizer_unittest: tokenizer.o tokenizer2.o tokenizer3.o tokenizer4.o tokenizer5.o tokenizer_util.o xml_tokenizer_unittest.o
	g++ -g tokenizer.o tokenizer2.o tokenizer3.o tokenizer4.o tokenizer5.o tokenizer_util.o xml_tokenizer_unittest.o ../unicode/libunicode.a ../libgb.a -lm -lpthread -lssl -lcrypto -lz -lpcre -lsqlite3 -ldl -L../ -lcld2_full -lcld3 -lprotobuf -lced -lcares -o $@

PHONY: run
run: tokenizer_unittest
	LD_LIBRARY_PATH=.. ./tokenizer_unittest

PHONY: xml_run
xml_run: xml_tokenizer_unittest
	LD_LIBRARY_PATH=.. ./xml_tokenizer_unittest


.PHONY: clean
clean:
	rm -f *.o
	rm -f libtokenizer.a
	rm -f tokenizer_unittest xml_tokenizer_unittest
	rm -f core


tokenizer.o:  tokenizer.cpp  tokenizer.h
tokenizer2.o: tokenizer2.cpp tokenizer.h
tokenizer3.o: tokenizer3.cpp tokenizer.h
tokenizer4.o: tokenizer4.cpp tokenizer.h
tokenizer5.o: tokenizer5.cpp tokenizer.h
tokenizer_util.o: tokenizer_util.cpp tokenizer_util.h tokenizer.h
