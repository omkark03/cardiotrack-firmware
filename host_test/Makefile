# Laptop tests. Needs only g++ (C++17).
#
#   make              unit tests for the portable modules (HR algorithm, payload, quality)
#   make sim          runs the REAL firmware logic against stubbed hardware, 3 scenarios:
#                       sim_bench   mock sensors, Wi-Fi/server/NTP failures, outage queue
#                       sim_sleep   mock sensors, real deep-sleep path, motion wake, power loss
#                       sim_nosens  real sensor drivers on an empty I2C bus (graceful failure)
#   make OUT=dir      where body.bin / boundary.txt / dumped uploads go
OUT ?= .

CORE = ../src/core/hr_algo.cpp ../src/core/quality.cpp ../src/core/payload.cpp
APP  = ../src/main.cpp ../src/app/app.cpp ../src/services/burst.cpp ../src/services/queue.cpp ../src/services/net.cpp \
       ../src/services/sleep_mgr.cpp ../src/hal/factory.cpp
INC  = -Istubs -I../src -I../include
CXX  = g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -O1
BOARD = -DBOARD_BREADBOARD_V1

test: test_core
	./test_core $(OUT)

test_core: $(CORE) test_core.cpp
	$(CXX) -I../src -I../include $(CORE) test_core.cpp -o test_core -lm

sim: sim_bench sim_sleep sim_nosens
	./sim_bench > sim_bench.log; tail -14 sim_bench.log
	./sim_sleep > sim_sleep.log; tail -14 sim_sleep.log
	./sim_nosens > sim_nosens.log; tail -14 sim_nosens.log

sim_bench: $(CORE) $(APP) sim_main.cpp
	$(CXX) $(INC) $(BOARD) -DUSE_MOCK_SENSORS -DBENCH_MODE -DCFG_BURST_SEC=10 -DCFG_CYCLE_SEC=30 $(CORE) $(APP) sim_main.cpp -o $@ -lm

sim_sleep: $(CORE) $(APP) sim_main.cpp
	$(CXX) $(INC) $(BOARD) -DUSE_MOCK_SENSORS $(CORE) $(APP) sim_main.cpp -o $@ -lm

sim_nosens: $(CORE) $(APP) sim_main.cpp
	$(CXX) $(INC) $(BOARD) -DBENCH_MODE -DSIM_NO_SENSORS -DCFG_BURST_SEC=10 -DCFG_CYCLE_SEC=30 $(CORE) $(APP) sim_main.cpp -o $@ -lm

clean:
	rm -rf test_core sim_bench sim_sleep sim_nosens sim_flash *.log
