CPPFLAGS += -std=c++17 -msse3 -mfpmath=sse -ffp-contract=off -fno-fast-math
CPPFLAGS += -I ../../../external/robin_hood

CPPFLAGS += -g

#CPPFLAGS += -O3 -DNDEBUG=1


bin/% : src/%.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) -o $@ $^

all_test : bin/generate_izhikevich_sparse bin/generate_CUBA \
	bin/reference_simulator \
	bin/test_dumb_snn_files bin/test_sink_snn bin/test_source_snn bin/test_source_sink_inmem