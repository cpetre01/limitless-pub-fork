#!/usr/bin/env bash

remote_nodes=$1

if [[ -z "$remote_nodes" ]]; then
    echo "Usage: $0 <remote_nodes>"
    exit 1
fi

SERVICE=limitless.service

echo "Starting LIMITLESS Monitoring Service on $remote_nodes"
psh $remote_nodes systemctl enable $SERVICE
psh $remote_nodes systemctl start $SERVICE
psh $remote_nodes systemctl status $SERVICE
