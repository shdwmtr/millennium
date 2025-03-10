# Build Millennium inside a docker container
# Author: github.com/LazyHatGuy
# Licence: MIT

FROM ubuntu:jammy AS build

RUN set -ex && useradd -ms /bin/bash -d /home/runner runner \
    && dpkg --add-architecture i386 \
    && apt update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends tzdata \
    && apt install -y \
                build-essential \
                zlib1g-dev:i386 \
                libncurses5-dev:i386 \
                libgdbm-dev:i386 \
                libnss3-dev:i386 \
                libssl-dev:i386 \
                libreadline-dev:i386 \
                libffi-dev:i386 \
                wget \
                curl zip unzip tar \
                libbz2-dev:i386 \
                libsqlite3-dev:i386 \
                liblzma-dev:i386 \
                gcc-multilib \
                g++-multilib \
                libgtk-3-dev ninja-build linux-libc-dev gdb lcov pkg-config \
                libbz2-dev libffi-dev libgdbm-dev libgdbm-compat-dev liblzma-dev \
                libncurses5-dev libreadline6-dev libsqlite3-dev libssl-dev \
                lzma lzma-dev tk-dev uuid-dev zlib1g-dev libmpdec-dev git

# Build CMake
RUN set -ex && cd /tmp \
    && wget https://github.com/Kitware/CMake/releases/download/v3.21.1/cmake-3.21.1.tar.gz \
    && tar -xvf cmake-3.21.1.tar.gz \
    && cd cmake-3.21.1 \
    && ./bootstrap \
    && gmake \
    && gmake install \
    && cmake --version \
    && rm -rfv /tmp/*
    
# Build Python
RUN set -ex && cd /home/runner \
    && wget https://www.python.org/ftp/python/3.11.8/Python-3.11.8.tgz \
    && tar -xvf Python-3.11.8.tgz \
    && mkdir -p /home/runner/.millennium/ext/data/cache \
    && cd Python-3.11.8 \
    && CFLAGS="-m32" LDFLAGS="-m32" ./configure --prefix=/home/runner/.millennium/ext/data/cache --enable-optimizations \
    && make -j$(nproc) \
    && make altinstall
    
# Move dependencies
RUN set -ex && mkdir -p /home/runner/.millennium/ext/data/cache/lib/tmp \
    && cd /home/runner/.millennium/ext/data/cache/lib/tmp \
    && ar -x ../libpython3.11.a \
    && gcc -m32 -shared -o ../libpython-3.11.8.so *.o \
    && cd /home/runner/.millennium/ext/data/cache/lib \
    && rm -rvf tmp \
    && mkdir -p /home/runner/Documents/LibPython/ \
    && cd /home/runner/.millennium/ext/data/cache/include/python3.11/ \
    && mv * /home/runner/Documents/LibPython/ \
    && rm -rvf /home/runner/.millennium/ext/data/cache/lib/python3.11/test/ \
    && rm -rvf /home/runner/.millennium/ext/data/cache/share \
    && rm -rvf /home/runner/.millennium/ext/data/cache/include \
    && rm -rvf /home/runner/.millennium/ext/data/cache/lib/python3.11/__pycache__/ \
    && rm -rvf /home/runner/.millennium/ext/data/cache/lib/python3.11/config-3.11-x86_64-linux-gnu/ \
    && rm /home/runner/.millennium/ext/data/cache/lib/libpython3.11.a \
    && mv /home/runner/.millennium/ext/data/cache/lib/libpython-3.11.8.so /home/runner/.millennium/libpython-3.11.8.so \
    && mkdir -p /home/runner/env/ext/data \
    && cp -rv /home/runner/.millennium/ext/data/cache /home/runner/env/ext/data \
    && mkdir -p /opt/python-i686-3.11.8/lib/ \
    && cp -v /home/runner/.millennium/libpython-3.11.8.so /opt/python-i686-3.11.8/lib/libpython-3.11.8.so

WORKDIR /home/runner/work/Millennium/Millennium

COPY . .
RUN chown -R runner:runner /home/runner

USER runner
SHELL ["/bin/bash", "-c"]
ENV FNM_PATH="/home/runner/.local/share/fnm"

# Setup NPM and build Millennium
RUN set -ex \
    && echo -e "# current version of millennium\nDEV-Docker-Build" > version \
    && curl -o- https://fnm.vercel.app/install | bash \
    && export PATH="$FNM_PATH:$PATH" \
    && eval "`fnm env`" \
    && env \
    && fnm install 22 \
    && cd assets \
    && npm install \ 
    && npm run build \
    && cd .. \
    && bash scripts/ci/posix/mk-assets.sh \
    && ./vendor/vcpkg/bootstrap-vcpkg.sh -disableMetrics && ./vendor/vcpkg/vcpkg integrate install \
    && cmake --preset=linux-debug -DGITHUB_ACTION_BUILD=OFF \
    && cmake --build build \
    && mkdir -p /home/runner/env/ext/data/shims \
    && cp -v build/libmillennium_x86.so /home/runner/env/ \
    && cp -v ~/.millennium/libpython-3.11.8.so /home/runner/env/libpython-3.11.8.so \
    && cp -v scripts/posix/start.sh /home/runner/env/start.sh \
    && npm install @steambrew/api \
    && cp -v node_modules/@steambrew/api/dist/webkit_api.js /home/runner/env/ext/data/shims/webkit_api.js \
    && cp -v node_modules/@steambrew/api/dist/client_api.js /home/runner/env/ext/data/shims/client_api.js

#  Restructure Filesystem and compress to build-millennium.tar.gz
WORKDIR /home/runner
RUN set -ex \
    && mkdir -p build/usr/lib/millennium \
    && cp -rv env/*.so build/usr/lib/millennium/ \
    && mkdir build/usr/bin \
    && cp -v work/Millennium/Millennium/build/cli/millennium build/usr/bin/ \
    && chmod +x build/usr/bin/millennium \
    && mkdir -p build/home/user/.local/share/millennium \
    && cp -rv env/ext/data/shims build/home/user/.local/share/millennium/lib/ \
    && cp -rv env/ext/data/cache build/home/user/.local/share/millennium/lib/ \
    && cp -rv env/ext/data/assets build/home/user/.local/share/millennium/lib/ \
    && tar -czvf build-millennium.tar.gz build