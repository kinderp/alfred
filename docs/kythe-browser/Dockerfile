FROM ubuntu:24.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        bash \
        build-essential \
        ca-certificates \
        findutils \
        make \
        python3 \
        coreutils && \
    rm -rf /var/lib/apt/lists/*
