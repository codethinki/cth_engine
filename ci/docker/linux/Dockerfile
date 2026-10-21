FROM cachyos/cachyos:latest

# vulkan-swrast: lavapipe, the cpu vulkan driver of mesa
# vulkan-validation-layers: debug builds enable them with synchronization validation
# shaderc: glslc for the shader compile tests
RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
    base-devel \
    cmake \
    ninja \
    git \
    gcc \
    clang \
    lld \
    python \
    curl \
    wget \
    zip \
    unzip \
    tar \
    mono \
    buildcache-git \
    vulkan-icd-loader \
    vulkan-swrast \
    vulkan-validation-layers \
    shaderc

# the tests run on lavapipe, also if another driver is installed
ENV VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
