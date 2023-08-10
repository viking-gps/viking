ARG UBUNTU_VERSION=22.04

FROM ubuntu:$UBUNTU_VERSION

ARG LIBGTK=libgtk-3-dev

ENV DEBIAN_FRONTEND=noninteractive
RUN apt update && \
    apt install -y build-essential gtk-doc-tools docbook-xsl yelp-tools libpng-dev $LIBGTK libicu-dev libjson-glib-dev intltool autopoint xxd libcurl4-gnutls-dev libglib2.0-dev-bin libsqlite3-dev nettle-dev libmapnik-dev libgeoclue-2-dev libgexiv2-dev libgps-dev libmagic-dev libbz2-dev libzip-dev liboauth-dev libnova-dev && \
    apt install -y sudo && \
    rm -rf /var/lib/apt/lists/*
