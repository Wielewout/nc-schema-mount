#!/bin/bash

set -e

export NETOPEER2_USERS="${NETOPEER2_USERS:-user:password:pass}"

export SYSREPO_REPOSITORY_PATH="/etc/sysrepo"

echo "Starting netopeer2 initialization"

echo "Starting sysrepo-plugind..."
if pgrep -x sysrepo-plugind >/dev/null 2>&1; then
    echo "sysrepo-plugind already running."
else
    sysrepo-plugind -d -v 2 &
fi

echo "Waiting for sysrepo-plugind to be ready..."
until sysrepoctl -l > /dev/null 2>&1; do
    printf "."
    sleep 1
done
echo "Sysrepo-plugind is ready"

rm -f /etc/ssh/ssh_host_*_key*

export NP2_MODULE_DIR="/usr/share/yang/modules/netopeer2"
export LN2_MODULE_DIR="/usr/share/yang/modules/libnetconf2"
export NP2_MODULE_PERMS="0644"

echo "Running netopeer2 setup..."
/usr/share/netopeer2/scripts/setup.sh

echo "Running netopeer2 merge hostkey..."
/usr/share/netopeer2/scripts/merge_hostkey.sh

echo "Waiting for sysrepo to be ready"
sleep 3

echo "Setting up users"

USER_CONFIG_XML=""
if [ -n "$NETOPEER2_USERS" ]; then
    for user_data in $NETOPEER2_USERS; do
        username=$(echo "$user_data" | cut -d: -f1)
        authtype=$(echo "$user_data" | cut -d: -f2)
        authdata=$(echo "$user_data" | cut -d: -f3)

        echo "Configuring user '$username' with auth type '$authtype'"

        if [ "$authtype" = "pubkey" ]; then
            USER_CONFIG_XML="$USER_CONFIG_XML
<user>
    <name>$username</name>
    <public-keys>
        <inline-definition>
            <public-key>
                <name>key-for-$username</name>
                <public-key-format xmlns:ct=\"urn:ietf:params:xml:ns:yang:ietf-crypto-types\">ct:ssh-public-key-format</public-key-format>
                <public-key>$authdata</public-key>
            </public-key>
        </inline-definition>
    </public-keys>
</user>"
        elif [ "$authtype" = "password" ]; then
            USER_CONFIG_XML="$USER_CONFIG_XML
<user>
    <name>$username</name>
    <password>\$0\$$authdata</password>
</user>"
        else
            echo "Warning: Unknown auth type '$authtype' for user '$username'"
        fi
    done
fi

if [ -z "$USER_CONFIG_XML" ]; then
    echo "NETOPEER2_USERS not set or empty, exiting"
    exit 1
fi

echo "Configuring netconf server..."
cat > /tmp/auth_config.xml << EOF
<netconf-server xmlns="urn:ietf:params:xml:ns:yang:ietf-netconf-server">
    <listen>
        <endpoints>
            <endpoint>
                <name>default-ssh</name>
                <ssh>
                    <tcp-server-parameters>
                        <local-address>0.0.0.0</local-address>
                    </tcp-server-parameters>
                    <ssh-server-parameters>
                        <server-identity>
                            <banner xmlns="urn:cesnet:libnetconf2-netconf-server">netopeer2-netconf-server</banner>
                            <host-key>
                                <name>default-key</name>
                                <public-key>
                                    <central-keystore-reference>genkey</central-keystore-reference>
                                </public-key>
                            </host-key>
                        </server-identity>
                        <client-authentication>
                            <users>
                                $USER_CONFIG_XML
                            </users>
                        </client-authentication>
                    </ssh-server-parameters>
                </ssh>
            </endpoint>
        </endpoints>
    </listen>
</netconf-server>
EOF

if sysrepocfg --edit=/tmp/auth_config.xml -d startup -m ietf-netconf-server -f xml -v 2; then
    echo "User authentication configured successfully in startup"
    if sysrepocfg --copy-from=startup -m ietf-netconf-server -v 2; then
        echo "ietf-netconf-server config copied from startup to running successfully"
    else
        echo "WARNING: Failed to copy ietf-netconf-server startup to running"
    fi
else
    echo "Warning: Could not configure authentication"
fi
rm -f /tmp/auth_config.xml

echo "Initializing NACM..."
cat > /tmp/nacm.xml << EOF
<nacm xmlns="urn:ietf:params:xml:ns:yang:ietf-netconf-acm">
  <enable-nacm>true</enable-nacm>
  <enable-external-groups>false</enable-external-groups>
  <read-default>permit</read-default>
  <write-default>permit</write-default>
  <exec-default>permit</exec-default>
</nacm>
EOF
sysrepocfg -v3 -d startup --module ietf-netconf-acm --edit /tmp/nacm.xml
sysrepocfg -v3 --copy-from startup --module ietf-netconf-acm
rm -f /tmp/nacm.xml

echo "Netopeer2 initialization complete" 

echo "Starting netopeer2 server"
netopeer2-server -d -v 2
