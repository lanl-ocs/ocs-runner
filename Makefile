PKG_CONFIG_PATH = $(SPDK_DIR)/build/lib/pkgconfig

CFLAGS   := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --cflags spdk_nvme spdk_env_dpdk)
SPDK_LIB := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --libs spdk_nvme)
DPDK_LIB := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --libs spdk_env_dpdk)
SYS_LIB  := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --libs --static spdk_syslibs)

TARGET=ocs-runner

all: ${TARGET}

${TARGET}: runner.cc
	$(CXX) -O2 -DNDEBUG $(CFLAGS) -o $@ $^ -pthread -Wl,--allow-multiple-definition -Wl,-Bstatic -Wl,--whole-archive $(SPDK_LIB) $(DPDK_LIB) -Wl,--no-whole-archive -Wl,-Bdynamic $(SYS_LIB)

clean:
	rm -f $(TARGET)
