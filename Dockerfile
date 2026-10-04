FROM ubuntu:24.04
LABEL authors="commkicks"

ENV PATH="/usr/local/bin:${PATH}"

RUN apt-get update  \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    ninja-build \
    gdb \
    git \
    file \
    pkg-config \
    python3-pip \
    openssh-server \
    libdbus-1-dev \
    libgtk-3-dev \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-net-dev \
    libsdl2-ttf-dev \
    libphysfs-dev \
    rapidjson-dev  \
    rsync \
    git \
 && pip3 install --break-system-packages --no-cache-dir cmake \
 && ln -sf /usr/local/bin/cmake /usr/bin/cmake \
 && mkdir -p /run/sshd \
 && useradd -m -s /bin/bash dev \
 && echo 'dev:dev' | chpasswd \
 && rm -rf /var/lib/apt/lists/* \
 && git clone https://github.com/catchorg/Catch2.git \
 && cd Catch2 \
 && cmake -B build -S . -DBUILD_TESTING=OFF \
 && cmake --build build/ --target install

EXPOSE 22

CMD ["/usr/sbin/sshd", "-D"]