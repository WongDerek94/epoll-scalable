# Output Name
OUTPUT 				= client# executable
ERROR_FILE			= error.txt# error output file

# Configuration parameters
HOST				= 192.168.0.106# host address
PORT				= 7000# server port 
SEND_COUNT			= 60# number of send per client
MSG 				= "Hello, World!"# sending message
DELAY_MS			= 250# in milliseconds max 999999999

# Testing parameters
N_CLIENT_TICK		= 1000
N_TICK_FIXED		= 10

$(OUTPUT):
	gcc -Wall $(OUTPUT).c -o $(OUTPUT) -fopenmp

# Targets
all: clean $(OUTPUT)

run:
	./$(OUTPUT) --host $(HOST) --port $(PORT) --message $(MSG) --count $(SEND_COUNT) \
	--delay $(DELAY_MS)

clean:
	rm -f $(OUTPUT)

# Notes
# 1. If you've run previous tests, ensure that there are no zombie clients remaining before starting new test.
#
# 2. Run this command to kill zombie clients
# kill $(pgrep client)
# use top to view zombie process   

test:
	-kill $$(pgrep $(OUTPUT)) 2>> /dev/null
	ulimit -n 120000
	i=0
	echo "" > $(ERROR_FILE) ; \
	for number in `seq 1 $(N_TICK_FIXED)` ; do \
		for number in `seq 1 $(N_CLIENT_TICK)` ; do \
			./$(OUTPUT) --host $(HOST) --port $(PORT) --message $(MSG) --count $(SEND_COUNT) \
			--delay $(DELAY_MS) 2>> $(ERROR_FILE) & \
		done ; \
		((i+=$(N_CLIENT_TICK))); \
		echo "$$i client(s) spawned"; \
	done ;

test-clean: # Kill zombie process
	-kill $$(pgrep $(OUTPUT)) 2>> /dev/null

watch:
	while true; do \
		netstat -anp --ip ; \
		sleep 1 & \
		wait $$i ; \
        clear ; \
	done ; \