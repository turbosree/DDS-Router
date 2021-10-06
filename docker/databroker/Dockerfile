FROM ubuntu:focal

WORKDIR /databroker

# Needed for a dependency that force to set timezone
ENV TZ=Europe/Madrid
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Get ubuntu repositories information and install dependencies
RUN apt-get update && apt-get install -y \
        git \
        build-essential \
        cmake \
        libssl-dev \
        libasio-dev \
        libtinyxml2-dev \
        libyaml-cpp-dev \
        wget \
        python3-pip && \
    pip3 install colcon-common-extensions vcstool

RUN echo '{  \n\
    "names": \n\
    { \n\
        "databroker": \n\
        { \n\
            "cmake-args": \n\
            [ \n\
                "-DCMAKE_BUILD_TYPE=Debug", \n\
                "-DLOG_INFO=ON" \n\
            ] \n\
        } \n\
    } \n\
}' >> colcon.meta && \
    mkdir resources

# Download and build Databroker
RUN wget https://raw.githubusercontent.com/eProsima/databroker/main/databroker.repos && \
    mkdir src && \
    vcs import src < databroker.repos && \
    cd src/fastrtps && git checkout feature/allow-ds-without-port && cd ../.. && \
    colcon build --event-handlers=console_direct+

CMD [ "./build/databroker/databroker", "--configuration-file", "resources/DATABROKER_CONFIGURATION.yaml" ]
