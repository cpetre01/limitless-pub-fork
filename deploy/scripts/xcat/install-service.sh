#!/usr/bin/env bash

PROJECT_ROOT_PATH=/opt/admin/limitless-pub-fork
SERVICE_INSTALL_PREFIX=/etc/systemd/system

PYTHON_PROGRAM=room_temp_monitor.py
INIT_FILE=init.dae
BRANCHES=(prod testing)
branch=$1
remote_nodes=$2

if [[ -z "$branch"  || ! " ${BRANCHES[@]} " =~ " $branch " || -z "$remote_nodes" ]]; then
    echo "Usage: $0 <"${BRANCHES[0]}"|"${BRANCHES[1]}"> <remote_nodes>"
    exit 1
fi

SERVICE=limitless.service
CODE_INSTALL_PREFIX=/opt/limitless
CONFIG_INSTALL_PREFIX=/var/lib/limitless


echo "Installing LIMITLESS Monitoring Service ($branch) on $remote_nodes"

echo "Uninstalling previous version"
psh $remote_nodes systemctl stop $SERVICE
psh $remote_nodes systemctl disable $SERVICE


echo "Installing new version"
# install the init file
xdcp $remote_nodes $PROJECT_ROOT_PATH/deploy/init-"$branch".dae $CONFIG_INSTALL_PREFIX/$INIT_FILE
psh $remote_nodes 'NODE_IP=$(hostname -I | tr " " "\n" | grep "^10\.119\.12\." | head -n 1) && sed -i "/# server ip/{n;s/.*/$NODE_IP/}" '"${CONFIG_INSTALL_PREFIX}"'/'"${INIT_FILE}"''
psh $remote_nodes chown limitless:limitless $CONFIG_INSTALL_PREFIX/$INIT_FILE
psh $remote_nodes chmod 0600 $CONFIG_INSTALL_PREFIX/$INIT_FILE


# install systemd files
xdcp $remote_nodes $PROJECT_ROOT_PATH/deploy/$SERVICE $SERVICE_INSTALL_PREFIX


psh $remote_nodes systemctl daemon-reload
echo "Done"
