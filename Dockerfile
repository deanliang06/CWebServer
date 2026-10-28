FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    bash \
    curl \
    gcc \
    make

EXPOSE 8000

WORKDIR /app

CMD ["/bin/bash"]