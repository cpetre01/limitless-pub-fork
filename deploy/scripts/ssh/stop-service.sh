#!/usr/bin/env bash

remote_host=$1

if [[ -z "$remote_host" ]]; then
    echo "Usage: $0 <remote_host>"
    exit 1
fi

SERVICE=limitless.service

echo "Stopping LIMITLESS Monitoring Service on $remote_host"
ssh $remote_host systemctl disable $SERVICE
ssh $remote_host systemctl stop $SERVICE
ssh $remote_host systemctl status $SERVICE
