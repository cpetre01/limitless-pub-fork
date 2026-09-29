#!/usr/bin/env bash

PROJECT_ROOT_PATH=/opt/admin/limitless-pub-fork
SERVICE_INSTALL_PREFIX=/etc/systemd/system

PYTHON_PROGRAM=room_temp_monitor.py
INIT_FILE=init.dae
BRANCHES=(prod testing)
branch=$1
remote_host=$2

if [[ -z "$branch"  || ! " ${BRANCHES[@]} " =~ " $branch " || -z "$remote_host" ]]; then
    echo "Usage: $0 <"${BRANCHES[0]}"|"${BRANCHES[1]}"> <remote_host>"
    exit 1
fi

SERVICE=limitless.service
CODE_INSTALL_PREFIX=/opt/limitless
CONFIG_INSTALL_PREFIX=/var/lib/limitless


echo "Installing LIMITLESS Monitoring Service ($branch) on $remote_host"

echo "Uninstalling previous version"
ssh $remote_host systemctl stop $SERVICE
ssh $remote_host systemctl disable $SERVICE


echo "Installing new version"
# install the init file
scp $PROJECT_ROOT_PATH/deploy/init-"$branch".dae $remote_host:$CONFIG_INSTALL_PREFIX/$INIT_FILE
ssh $remote_host 'NODE_IP=$(hostname -I | tr " " "\n" | grep "^10\.119\.12\." | head -n 1) && sed -i "/# server ip/{n;s/.*/$NODE_IP/}" '"${CONFIG_INSTALL_PREFIX}"'/'"${INIT_FILE}"''
ssh $remote_host chown limitless:limitless $CONFIG_INSTALL_PREFIX/$INIT_FILE
ssh $remote_host chmod 0600 $CONFIG_INSTALL_PREFIX/$INIT_FILE


# install systemd files
scp $PROJECT_ROOT_PATH/deploy/$SERVICE $remote_host:$SERVICE_INSTALL_PREFIX


ssh $remote_host systemctl daemon-reload
echo "Done"
