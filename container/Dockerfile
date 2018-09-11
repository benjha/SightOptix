#Build:
#       sudo nvidia-docker build -t "sight:9.0-devel-ubuntu16.04" .
#Run:
#       sudo nvidia-docker run -it -p 9002:9002 sight:9.0-devel-ubuntu16.04 bash


FROM nvidia/cuda:9.0-devel-ubuntu16.04

LABEL maintainer "Benjha"

RUN apt-get update && apt-get install -y --no-install-recommends \
        libpng12-dev \
        ca-certificates \
        git \
        g++ \
        vim \
        curl \
        openssh-client \
        cmake \
        cuda-samples-$CUDA_PKG_VERSION \
        libturbojpeg && cd /usr/lib/x86_64-linux-gnu && ln -s libturbojpeg.so.0 libturbojpeg.so &&\
        rm -rf /var/lib/apt/lists/*

ENV LIBRARY_PATH ${LD_LIBRARY_PATH}
#RUN ls -l /usr/local/nvidia/lib64
RUN echo $LIBRARY_PATH
RUN echo
RUN echo $LD_LIBRARY_PATH
RUN echo
RUN mkdir sight &&\
        cd sight &&\
        pwd &&\
        mkdir Programs &&\
        cd Programs &&\
# Optix distribution is not allowed, you need to get it from Nvidia
#       git clone https://code.ornl.gov/e8h/NVIDIA-OptiX-SDK-5.0.1-linux64.git &&\
        git clone https://code.ornl.gov/e8h/NVIDIA-OptiX-SDK-5.1.0-linux64.git &&\
        git clone https://github.com/NVIDIA/NvPipe.git &&\
#       cd NvPipe &&\
#       mkdir build &&\
#       cd build &&\
#       cmake .. -DNVPIPE_WITH_OPENGL=OFF &&\
#       make install &&\
# build
#        cd .. &&\
# NvPipe 
#        cd .. &&\
# Programs
        cd .. &&\
        mkdir code &&\
        cd code &&\
        git clone https://github.com/benjha/SightOptix.git &&\
        pwd

RUN echo $HOME

#using ${HOME} should  work, but it does not.
#ENV LD_LIBRARY_PATH ${HOME}/sight/Programs/NVIDIA-OptiX-SDK-5.0.1-linux64/lib64:/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}
ENV LD_LIBRARY_PATH /sight/Programs/NVIDIA-OptiX-SDK-5.1.0-linux64/lib64:/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}

RUN cd /sight/code/SightOptix/shaders &&\
        rm *.ptx && \
        nvcc -I../../../Programs/NVIDIA-OptiX-SDK-5.1.0-linux64/include -O3 -gencode arch=compute_70,code=sm_70 -ptx -m64 particles.cu &&\
        nvcc -I../../../Programs/NVIDIA-OptiX-SDK-5.1.0-linux64/include -O3 -gencode arch=compute_70,code=sm_70 -ptx -m64 shaders.cu &&\
        nvcc -I../../../Programs/NVIDIA-OptiX-SDK-5.1.0-linux64/include -O3 -gencode arch=compute_70,code=sm_70 -ptx -m64 shaders_denoiser.cu

RUN cd /sight/code/SightOptix/Release &&\
        sed -i 's/\/usr\/local\/cuda-8.0\/bin\/nvcc/nvcc/g' makefile &&\
        sed -i 's/\/usr\/local\/cuda-8.0\/bin\/nvcc/nvcc/g' source/subdir.mk &&\
        sed -i 's/\/usr\/local\/cuda-8.0\/bin\/nvcc/nvcc/g' frameserver/communications/asio/impl/subdir.mk &&\
        sed -i 's/\/usr\/local\/cuda-8.0\/bin\/nvcc/nvcc/g' frameserver/source/subdir.mk &&\
        sed -i 's/-gencode\ arch=compute_35,code=compute_35\ -gencode\ arch=compute_60,code=compute_60\ -gencode\ arch=compute_35,code=sm_35\ -gencode\ arch=compute_60,code=sm_60/-gencode arch=compute_70,code=compute_70/g' makefile &&\
        sed -i 's/-gencode\ arch=compute_35,code=sm_35\ -gencode\ arch=compute_60,code=sm_60/-gencode\ arch=compute_70,code=compute_70/g' source/subdir.mk &&\
        sed -i 's/-gencode\ arch=compute_35,code=sm_35\ -gencode\ arch=compute_60,code=sm_60/-gencode\ arch=compute_70,code=compute_70/g' frameserver/communications/asio/impl/subdir.mk
#        make clean &&\
#        make -j 16 &&\
#        mv SightOptix ../

WORKDIR /sight