#!/bin/bash

if [[ $1 == "listener" ]]
then
    NODE="listener"
else
    NODE="talker"
fi


source /opt/ros/foxy/setup.bash
ros2 run demo_nodes_cpp ${NODE}
