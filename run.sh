docker build -t ubuntu .
docker run -it --rm -v "${PWD}:/app" -w /app -p 8000:8000 ubuntu