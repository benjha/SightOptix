################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../source/Arcball.cpp \
../source/DeviceMemoryLogger.cpp \
../source/ImageLoader.cpp \
../source/PPMLoader.cpp \
../source/cOptixParticlesRenderer.cpp \
../source/loaders.cpp \
../source/main.cpp \
../source/sutil.cpp 

OBJS += \
./source/Arcball.o \
./source/DeviceMemoryLogger.o \
./source/ImageLoader.o \
./source/PPMLoader.o \
./source/cOptixParticlesRenderer.o \
./source/loaders.o \
./source/main.o \
./source/sutil.o 

CPP_DEPS += \
./source/Arcball.d \
./source/DeviceMemoryLogger.d \
./source/ImageLoader.d \
./source/PPMLoader.d \
./source/cOptixParticlesRenderer.d \
./source/loaders.d \
./source/main.d \
./source/sutil.d 


# Each subdirectory must supply rules for building sources it contributes
source/%.o: ../source/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -I../frameserver/header -I../frameserver/communications -I../../../Programs/NVIDIA-OptiX-SDK-5.0.1-linux64/include -O3 -std=c++11 -gencode arch=compute_35,code=sm_35 -gencode arch=compute_60,code=sm_60  -odir "source" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -I../frameserver/header -I../frameserver/communications -I../../../Programs/NVIDIA-OptiX-SDK-5.0.1-linux64/include -O3 -std=c++11 --compile  -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


