cc = g++ 
prom = update
deps = $(shell find ./ -name "*.h")
src = $(shell find ./ -name "*.cpp")
obj = $(src:%.c=%.o) 

$(prom): $(obj)
	$(cc) -g -O0 -o  $(prom) $(obj)

%.o: %.c $(deps)
	$(cc) -c $< -o $@
clean:
	rm -r $(prom)
