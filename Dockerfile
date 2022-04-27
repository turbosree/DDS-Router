# Dockerfile to build DDS router application as a docker image
# 
# Author: sreejith.naarakathil@gmail.com
#

# Alpine is chosen to build the image because it has good support for creating
# statically-linked, small programs. Currently the DDS router application is
# not statically linked due to build issues in DDS Router.
ARG DISTRO_VERSION=3.15
FROM alpine:${DISTRO_VERSION} AS base

# Create separate targets for each phase, this allows us to cache intermediate
# stages and makes the final deployment stage
# small as it contains only what is needed.
FROM base AS devtools

# Install the typical development tools and some additions:
#   - ninja-build is a backend for CMake that often compiles faster than
#     CMake with GNU Make.
#   - Install the necessary libraries required for DDS router application.
RUN apk update && \
    apk add \
        asio-dev \
        tinyxml2-dev \
        build-base \
        cmake \
        git \
        gcc \
        g++ \
        libc-dev \
        yaml-cpp-dev \
        ninja \
        openssl-dev \
        openssl-libs-static \
	py3-sphinx \
	py3-sphinx_rtd_theme
        
# Copy the source code to /app and compile it.
FROM devtools AS build
WORKDIR /app
COPY . /app
RUN rm -rf build install log
RUN mkdir -p build/foonathan_memory_vendor
RUN mkdir -p build/fastcdr
RUN mkdir -p build/fastdds
RUN mkdir -p build/ddsrouter

# Run the CMake in each dependency project and install it.
WORKDIR /app/build/foonathan_memory_vendor
RUN cmake -DCMAKE_BUILD_TYPE=Release /app/src/foonathan_memory_vendor
RUN make && make install

WORKDIR /app/build/fastcdr
RUN cmake -DCMAKE_BUILD_TYPE=Release /app/src/fastcdr
RUN make && make install

WORKDIR /app/build/fastdds
RUN cmake -DCMAKE_BUILD_TYPE=Release /app/src/fastdds
RUN make && make install

WORKDIR /app/build/ddsrouter
RUN cmake -DCMAKE_BUILD_TYPE=Release /app/src/ddsrouter
RUN make && make install

# Create the final deployment image, using `alpine`as the starting point.
FROM alpine:${DISTRO_VERSION} AS final
RUN apk update && \
    apk add \
        tinyxml2 \
	libstdc++ \
	libgcc \
	yaml-cpp

# Copy the program and shared libraries from the previously created stage and make it the entry point.
COPY --from=build /usr/local/bin/ddsrouter /
COPY --from=build /usr/local/share/resources/configurations/examples/change_domain_shapes_demo.yaml /DDS_ROUTER_CONFIGURATION.yaml
COPY --from=build /usr/local/lib/libfoonathan_memory* /usr/local/lib/
COPY --from=build /usr/local/lib/libfastcdr* /usr/local/lib/
COPY --from=build /usr/local/lib/libfastrtps* /usr/local/lib/

ENTRYPOINT ["./ddsrouter"]
CMD ["-c", "/DDS_ROUTER_CONFIGURATION.yaml"]
