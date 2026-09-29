#!/usr/bin/env bash

remote_nodes=$1

if [[ -z "$remote_nodes" ]]; then
    echo "Usage: $0 <remote_nodes>"
    exit 1
fi

SERVICE=limitless.service

echo "Stopping LIMITLESS Monitoring Service on $remote_nodes"
psh $remote_nodes systemctl disable $SERVICE
psh $remote_nodes systemctl stop $SERVICE
psh $remote_nodes systemctl status $SERVICE
