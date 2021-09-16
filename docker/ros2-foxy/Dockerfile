FROM osrf/ros:foxy-desktop

COPY ./run.bash /
RUN chmod +x /run.bash


ENTRYPOINT ["/run.bash"]
CMD ["talker"]
