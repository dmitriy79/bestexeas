FROM ubuntu:14.04
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 8842CE5E
RUN echo deb http://ppa.launchpad.net/bitcoin/bitcoin/ubuntu trusty main >/etc/apt/sources.list.d/bitcoin.list
RUN apt-get update # 2015-06-14
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections
RUN apt-get install -y libboost-filesystem1.54.0 libboost-program-options1.54.0 libboost-thread1.54.0 libdb4.8++ libcurl4-openssl-dev iptables
RUN apt-get install -y git build-essential g++ libboost-all-dev libdb4.8++-dev libqrencode-dev libssl-dev libcurl4-openssl-dev
COPY bcexchange.conf /root/.bcexchange/
