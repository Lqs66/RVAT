CXXFLAGS := -Wall -Wextra -O2 -fPIC -fvisibility=hidden
LDFLAGS := -static -lpthread

OBJS := heaptrack.o
TARGET_LIB := libheaptrack.a
TEST1_TARGET := memtest1
TEST2_TARGET := memtest2

all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) -target aarch64-linux-gnu $(CXXFLAGS) -c $< -o $@

$(TEST2_TARGET):
	g++ -static test2.cpp -L. -lheaptrack -lpthread -o memtest2

test: $(TEST2_TARGET)
	./$(TEST2_TARGET)

clean:
	rm -f $(OBJS) $(TARGET_LIB) $(TEST1_TARGET) $(TEST2_TARGET)

.PHONY: all clean

#gcc -static -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free test.c -L. -lmemtrack -lpthread -o memtest
#gcc -static test.c -lpthread -o memtest