NVCC = nvcc
NVCCFLAGS = -arch=compute_52

all:
	$(NVCC) $(NVCCFLAGS) main.cu graph.cu -o v5_iterative_gpu_c

clean:
	rm -f v5_iterative_gpu_c
