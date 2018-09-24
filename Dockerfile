FROM ubuntu:16.04
MAINTAINER Timur Garifullin
RUN apt-get update
RUN apt-get -y install gcc
RUN apt-get -y install git
RUN git clone https://github.com/init/http-test-suite.git
ADD ./ ./http-test-suite
WORKDIR ./http-test-suite
RUN gcc server.c -o server
EXPOSE 80
CMD ./server
