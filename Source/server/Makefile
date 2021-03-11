# Configuration parameters
OUTPUT=server# Output Name
PORT=7000# Server listening port
MAX_CORES=6# Number of threads to parallelize listening, IO, and data processing

# Targets
$(OUTPUT):
	gcc -Wall $(OUTPUT).c -o $(OUTPUT) -fopenmp

all: clean $(OUTPUT) run

run:
	ulimit -n 120000
	./$(OUTPUT) --port $(PORT) --max-core $(MAX_CORES)

clean:
	rm -f $(OUTPUT)

watch:
	while true; do \
		netstat -anp --ip ; \
		sleep 1 & \
		wait $$! ; \
        clear ; \
	done ; \
