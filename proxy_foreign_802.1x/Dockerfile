FROM ubuntu:18.04

# Install python
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    add-apt-repository ppa:deadsnakes/ppa && \
    apt-get update && \
    apt-get install -y python3.8 python3.8-venv

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# Create the virtual environment.
ENV VIRTUAL_ENV=/opt/venv
RUN python3.8 -m venv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

# Copy the files.
COPY ./requirements.txt ./requirements.txt

# Install proxy dependencies.
RUN pip install -r requirements.txt

# Install various programs.
RUN apt-get install -y net-tools && \
    apt-get install -y tcpdump && \
    apt-get install -y curl

RUN apt-get install -y build-essential

RUN apt-get install -y libssl-dev checkinstall pkg-config
RUN apt-get install -y libdbus-1-dev libdbus-glib-1-2 libdbus-glib-1-dev libreadline-dev libncurses5-dev
RUN apt-get install -y libnl-genl-3-dev libnl-3-dev

COPY ./wpa_supplicant-2.9 ./wpa_supplicant-2.9

# Install eapol_test
RUN cd ./wpa_supplicant-2.9/wpa_supplicant && make eapol_test && cp eapol_test /bin

RUN apt-get install -y wpasupplicant

# Set Flask development mode.
ENV FLASK_ENV=development

# Disable SSL checks for development purposes.
ENV AUTHLIB_INSECURE_TRANSPORT=true
ENV CURL_CA_BUNDLE=""

# UGLY HACK!!
RUN /opt/venv/bin/python3.8 -m pip install --upgrade pip
RUN pip install --force requests

# Now, install OpenPANA. Proxy uses the PaC module.
COPY ./openpana /openpana

RUN apt-get clean && apt-get update && \
    apt-get install -y sed && \
    apt-get install -y libssl-dev libxml2-dev && \
    apt-get install -y gcc && \
    apt-get install -y build-essential automake autoconf libtool

RUN cd openpana && \
    ./autogen.sh && \
    ./configure --sysconfdir=/etc/openpana --enable-relay

RUN cd openpana/src && \
    sed -i '/^DBG_LINKOPT =.*/c\DBG_LINKOPT = ' Makefile

RUN cd openpana && \
    make && make install

# Install Free Radius (for virtual AS)
RUN apt-get update && apt-get install -y freeradius freeradius-rest
RUN chmod -R 777 /root
COPY ./freeradius /etc/freeradius/3.0
COPY ./freeradius_certs/ca.pem /etc/freeradius/3.0/certs/ca.pem
COPY ./freeradius_certs/server.key /etc/freeradius/3.0/certs/server.key
COPY ./freeradius_certs/server.pem /etc/freeradius/3.0/certs/server.pem

# Fix permissions for freeradius
RUN chmod 555 -R /etc/freeradius/3.0/

# Copy OpenPANA configurations
COPY ./openpana/modified_config.xml /etc/openpana/config.xml
COPY ./openpana/radius_conf/eap-tls/client/ca.pem /etc/openpana/ca.pem
COPY ./openpana/radius_conf/eap-tls/client/client.pem /etc/openpana/client.pem
COPY ./openpana/radius_conf/eap-tls/client/client.pk8 /etc/openpana/client.pk8

# Save TLS keys to a file for capturing TLS packets
ENV SSLKEYLOGFILE=/keylogs
