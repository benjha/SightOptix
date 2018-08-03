################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../frameserver/source/cBroadcastServer.cpp 

OBJS += \
./frameserver/source/cBroadcastServer.o 

CPP_DEPS += \
./frameserver/source/cBroadcastServer.d 


# Each subdirectory must supply rules for building sources it contributes
frameserver/source/%.o: ../frameserver/source/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -I../frameserver/header -I../frameserver/communications -I../../../Programs/NVIDIA-OptiX-SDK-5.0.1-linux64/include -I../../../Programs/include -O3 -std=c++11 -gencode arch=compute_35,code=sm_35 -gencode arch=compute_60,code=sm_60  -odir "frameserver/source" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -I../frameserver/header -I../frameserver/communications -I../../../Programs/NVIDIA-OptiX-SDK-5.0.1-linux64/include -I../../../Programs/include -O3 -std=c++11 --compile  -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


