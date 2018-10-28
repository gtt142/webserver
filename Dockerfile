FROM ubuntu:16.04
MAINTAINER Timur Garifullin
RUN apt-get update
RUN apt-get -y install gcc
RUN apt-get -y install libevent-dev
ADD . .
RUN gcc -o server *.c -lpthread -levent
EXPOSE 80
CMD ./server