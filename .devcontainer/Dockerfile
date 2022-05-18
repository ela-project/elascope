FROM registry.fedoraproject.org/fedora:35

RUN dnf install -y --refresh \
        git \
        python3 \
        make \
        cmake \
        arm-none-eabi-gcc-cs-c++ \
        arm-none-eabi-gcc-cs \
        arm-none-eabi-newlib \
        arm-none-eabi-binutils-cs \
        gcc \
        g++ \
        clang-tools-extra && \
    dnf clean all && \
    rm -rf \
        /var/cache/dnf/*

CMD ["/bin/bash"]
