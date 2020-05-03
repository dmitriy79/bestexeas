FROM ubuntu:14.04
RUN apt-get update # 2014-12-14
RUN apt-get install -y dante-server
COPY danted.conf /etc/
CMD /usr/sbin/danted
EXPOSE 1080
