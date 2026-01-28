CXXFLAGS := -Wall -Wextra -O2 -fPIC -fvisibility=hidden
LDFLAGS := -static -lpthread

OBJS := timeLogger.o
TARGET_LIB := libtimeLogger.a

all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) -target aarch64-linux-gnu $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET_LIB)

.PHONY: all clean